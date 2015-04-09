#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <numa.h>
/* #include "heapinfo.h" */

#define NAIF 0
#define TRANSPOSE 1
#define PAR_MODULO 2
#define PAR_BLOC 3

double *A,*B,*C,*D, *BT;
unsigned int  vsplits=0, hsplits=0, numa=0;
int Nthreads=1;
size_t sz;
pthread_t *threads;
typedef struct th_arg_t
{
    void *mat;
    unsigned int lrmin, lrmax, colrmin, colrmax;
    int tid;
    int numanode;
}*th_args;
th_args args;


void usage(char *s)
{
    printf("Usage : %s [options] -S size -s seed \n",s);
    printf("Options in \n");
    printf("-h      Display thi message and quit\n");
    printf("-v      Turn on verbose mode\n");
    printf("-V      Turn on verifications\n");
    printf("-n nb   Use nb threads, for par_* algorithms\n");
    printf("-N nb     Use libnuma for mappings on nb numa nodes\n");
    printf("-a algo Use one of the following algorithms:\n");
    printf("        naif, transpose, par_modulo, par_bloc\n");
    printf("        Default is naif\n");
}

void print_mat(double *M)
{
    for(unsigned int i=0;i<sz;i++)
    {
        for(unsigned int j=0;j<sz;j++)
        {
            printf("%g ", M[i*sz+j]);
        }
        printf("\n");
    }
    printf("\n");
}

void print()
{
    printf("A\n");
    print_mat(A);
    printf("B\n");
    print_mat(B);
    printf("C\n");
    print_mat(C);
}
void do_transpose(void)
{
    BT=(double *)malloc(sizeof(double)*sz*sz);
    /* VALGRIND_NAME_STRUCT(BT, "Bt"); */
    if(!BT)
        exit(1);
    //Do the transpose

    for(unsigned int i=0;i<sz;i++)
    {
        for(unsigned int j=0;j<sz;j++)
        {
            BT[i*sz+j]=B[j*sz+i];
        }
    }
}

void do_free(void)
{
    if(numa)
    {
        numa_free(A,sizeof(double)*sz*sz);
        numa_free(B,sizeof(double)*sz*sz);
        numa_free(C,sizeof(double)*sz*sz);
    }
    else
    {
        free(A);
        free(B);
        free(C);
    }
    if(D)
        free(D);
    if(BT)
        free(BT);
}

void do_malloc(int numa)
{
    if(!numa)
    {
        A=(double *)malloc(sizeof(double)*sz*sz);
        B=(double *)malloc(sizeof(double)*sz*sz);
        C=(double *)malloc(sizeof(double)*sz*sz);
    } else if(!vsplits)
    {
        //Interleave alloc
        A=(double *)numa_alloc_interleaved(sizeof(double)*sz*sz);
        B=(double *)numa_alloc_interleaved(sizeof(double)*sz*sz);
        C=(double *)numa_alloc_interleaved(sizeof(double)*sz*sz);
    }else
    {
        A=(double *)numa_alloc(sizeof(double)*sz*sz);
        B=(double *)numa_alloc_interleaved(sizeof(double)*sz*sz);
        C=(double *)numa_alloc(sizeof(double)*sz*sz);
    }
    /* printf("A,%lu,%lu\n",(unsigned long)A, sizeof(double)*sz*sz); */
    /* VALGRIND_NAME_STRUCT(A, "A"); */
    /* VALGRIND_NAME_STRUCT(B, "B"); */
    /* VALGRIND_NAME_STRUCT(C, "C"); */
}

void do_init_random(void)
{
    for(unsigned int i=0;i<sz;i++)
    {
        for(unsigned int j=0;j<sz;j++)
        {
            A[i*sz+j]=drand48();
            B[i*sz+j]=drand48();
            C[i*sz+j]=0;
        }
    }
}

void init_matrix(long int seed)
{
    srand48(seed);

    do_malloc(numa);
    if(!A || !B || !C )
        exit(1);
    if(!vsplits) //Wait for the numa placement before actually init if vsplits are defined
        do_init_random();
}

void do_mult(double *Res)
{
    //Very naive matrix multplication
    for(unsigned int i=0;i<sz;i++)
    {
        for(unsigned int j=0;j<sz;j++)
        {
            Res[i*sz+j]=0;
            for(unsigned int k=0;k<sz;k++)
            {
                Res[i*sz+j]+=A[i*sz+k]*B[k*sz+j];
            }
        }
    }
}

void do_mult_trans(double *Res)
{
    do_transpose();


    //A bit less naive mat mult
    for(unsigned int i=0;i<sz;i++)
    {
        for(unsigned int j=0;j<sz;j++)
        {
            Res[i*sz+j]=0;
            for(unsigned int k=0;k<sz;k++)
            {
                Res[i*sz+j]+=A[i*sz+k]*BT[j*sz+k];
            }
        }
    }
}

void *do_mult_par_bloc_thread(void *arg)
{
    th_args args=(th_args)arg;
    double *Res=args->mat;
    printf("thread %d alive  pid %d tid %ld  on node %d!\n", args->tid, getpid(), syscall(SYS_gettid), args->numanode);
    //Bind to the good node
    if(numa)
        numa_run_on_node(args->numanode);

    /* printf("Working on %p from [%d][%d] to [%d][%d]\n", Res, args->lrmin, */
    /* args->colrmin, args->lrmax, args->colrmax); */
    for(unsigned int lr=args->lrmin; lr<args->lrmax;lr++)
    {
        for(unsigned int k=0;k<sz;k++)
        {
            //         printf("Thread %d computing R[%d][%d]+=A[%d][k]*B[k][%d]\n",
            //                 args->tid, lr,cr,lr,cr);
            for(unsigned int cr=args->colrmin; cr<args->colrmax;cr++)
            {
                /* Tr: Res[lr*sz+cr]+=A[lr*sz+k]*BT[cr*sz+k]; */
                Res[lr*sz+cr]+=A[lr*sz+k]*B[k*sz+cr];
            }
        }
    }
    return NULL;
}

int l2(int n)
{
    int p=1, res=0;
    while(p<n)
    {
        ++res;
        p=p<<1;
    }
    return res;
}

//Launch threads for mat mult
void do_mult_par_bloc(double *Res)
{
    /* do_transpose(); */
    threads=malloc(sizeof(pthread_t)*Nthreads);
    void *res;
    for(unsigned int i=0;i<sz*sz;i++)
    {
        Res[i]=0;
    }
    //Split the result matrix
    args=malloc(sizeof(struct th_arg_t)*Nthreads);

    //Do the splits
    unsigned int hblocsz=sz/(hsplits+1), curhbloc=0;
    unsigned int vblocsz=sz/(vsplits+1), curvbloc=0;
    /* printf("vblocsz: %u hblocsz %u\n", vblocsz, hblocsz); */
    size_t totalvblocsz=sz*sz/(vsplits+1);
    unsigned int blocsPerNodes=0;
    int node=0;

    if(numa)
    {
        blocsPerNodes=(vsplits+1)/numa; // 6 splits, numa 2 => 3 blocs per nodes
        for(unsigned int i=0; i< vsplits+1;++i)
        {
            if(i>0 && i%blocsPerNodes==0)
                ++node;
            printf("Numa bloc from %lu, offset %lu size %lu, node: %d\n",(long unsigned)A+(i*totalvblocsz),
                    i*totalvblocsz, totalvblocsz, node);
            numa_tonode_memory(A+(i*totalvblocsz), i*totalvblocsz,node);
            printf("Numa bloc from %lu, offset %lu size %lu, node: %d\n",(long unsigned)C+(i*totalvblocsz),
                    i*totalvblocsz, totalvblocsz, node);
            numa_tonode_memory(A+(i*totalvblocsz), i*totalvblocsz,node);
        }
    }
    node=0;
    for(int i=0;i<Nthreads;i++)
    {
        args[i].mat=Res;
        args[i].tid=i;
        args[i].lrmin=curvbloc*vblocsz;
        args[i].colrmin=curhbloc*hblocsz;
        args[i].lrmax=(curvbloc+1)*vblocsz;
        args[i].colrmax=(curhbloc+1)*hblocsz;
        args[i].numanode=node;
        printf("th %d vbloc %d hbloc %d\n", i, curvbloc, curhbloc);

        //Compute on witch bloc will be the next thread
        if(curvbloc==vsplits)
        {
            ++curhbloc;
            curvbloc=0;
            node=0;
        }
        else
        {
            ++curvbloc;
            if(blocsPerNodes!=0 && curvbloc%blocsPerNodes==0)
                ++node;
        }
    }
    do_init_random();
    //Start all threads
    for(int i=0; i< Nthreads;i++)
    {
        pthread_create(threads+i, NULL,do_mult_par_bloc_thread,
                (void *)(args+i));
    }
    //Wait for them
    for(int i=0; i< Nthreads;i++)
    {
        pthread_join(threads[i], &res);
    }
}

void* do_mult_par_modulo_thread(void *arg)
{
    double *Res=((th_args)arg)->mat;
    int tid=((th_args) arg)->tid, nth=Nthreads;
    printf("thread %d alive  pid %d tid %ld !\n", tid, getpid(), syscall(SYS_gettid));
    if(numa)
        numa_run_on_node(args->tid);
    //Very unefficient parallel matrix multplication
    unsigned int ligr=0, colr=tid;
    while(ligr*sz+colr<sz*sz)
    {
        for(unsigned int k=0;k<sz;k++)
        {
            //Compute
            // Tr: Res[ligr*sz+colr]+=A[ligr*sz+k]*BT[colr*sz+k];
            Res[ligr*sz+colr]+=A[ligr*sz+k]*B[k*sz+colr];
        }
        colr+=nth;
        if(colr>=sz)
        {
            ligr++;
            colr=tid;
        }
    }

    return NULL;
}
//Launch threads for mat mult
void do_mult_par_modulo(double *Res)
{
    //do_transpose();
    threads=malloc(sizeof(pthread_t)*Nthreads);
    args=malloc(sizeof(struct th_arg_t)*Nthreads);
    void *res;
    //Start all threads
    for(unsigned int i=0;i<sz*sz;i++)
    {
        Res[i]=0;
    }
    for(int i=0; i< Nthreads;i++)
    {
        args[i].tid=i;
        args[i].mat=Res;
        pthread_create(threads+i, NULL,do_mult_par_modulo_thread, (void *)(args+i));
    }
    //Wait for them
    for(int i=0; i< Nthreads;i++)
    {
        pthread_join(threads[i], &res);
    }
}


int main(int argc, char *argv[])
{
    int opt, verbose=0, verify=0;
    long int seed=-1;
    int algo=NAIF;
    double exp_time ;
    struct timespec m_time_s, m_time_f ; \
        sz=0;
    //Options
    extern char *optarg;
    printf("Main pid %d\n", getpid());
    while((opt=getopt(argc, argv, "a:s:S:n:N:vVh"))!=-1)
    {
        switch(opt)
        {
            case 'a':
                //select the algorithm
                if(!strcmp(optarg,"naif"))
                {
                    algo=NAIF;
                }
                else if (!strcmp(optarg, "transpose"))
                {
                    algo=TRANSPOSE;
                }else if (!strcmp(optarg,"par_modulo"))
                {
                    algo=PAR_MODULO;
                }else if(!strcmp(optarg,"par_bloc"))
                {
                    algo=PAR_BLOC;
                }
                else
                {
                    fprintf(stderr, "Invalid algorithm '%c' \"%s\"\n", (char)opt,
                            optarg);
                    usage(argv[0]);
                    exit(1);
                }
                break;
            case 's':
                //seed
                seed=atol(optarg);
                break;
            case 'S':
                //size
                sz=atol(optarg);
                break;
            case 'v':
                verbose=1;
                break;
            case 'V':
                verify=1;
                break;
            case 'n':
                Nthreads=atoi(optarg);
                printf("using %d threads\n", Nthreads);
                break;
            case 'N':
                numa=atoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Invalid argument '%c'\n", (char)opt);
                usage(argv[0]);
                exit(1);
        }
    }
    if(sz==0 || seed==-1)
    {
        usage(argv[0]);
        exit(1);
    }
    if(algo==PAR_BLOC || algo==PAR_MODULO)
    {
        if(Nthreads < 2)
        {
            fprintf(stderr,
                    "Won't use %d threads for a parallel algorithm, abort\n",
                    Nthreads);
            usage(argv[0]);
            exit(1);
        }
        if(algo==PAR_BLOC)
        {
            //Compute splits
            if(sz%Nthreads!=0)
            {
                fprintf(stderr, "%lu%%%d!=0, to lazy to cut this matrix\n",sz, Nthreads);
                exit(1);
            }

            if(Nthreads > 4 && Nthreads%4 == 0)
            {
                hsplits=3; //Cut into four pieces
                vsplits=Nthreads/4-1; // Cut vertically
            }
            else if(Nthreads%2==0)
            {
                hsplits=Nthreads/2-1;
                vsplits=1;
            }
            else
            {
                fprintf(stderr, "To lazy to compute bloc for an odd number of threads (%d)\n", Nthreads);
                exit(1);
            }
            printf("Doing multiplications with %d vsplits and %d hsplits on %d numanodes\n", vsplits, hsplits, numa);
        }
    }else if(Nthreads!=1)
    {
        fprintf(stderr,
                "Won't use %d threads for a sequential algorithm, abort\n",
                Nthreads);
        usage(argv[0]);
        exit(1);
    }

    //initializations
    init_matrix(seed);
    if(verbose)
        print();

    clock_gettime(CLOCK_REALTIME, &m_time_s ) ;
    //Do the multiplication using the selected algorithm
    switch(algo)
    {
        case NAIF:
            do_mult(C);
            break;
        case TRANSPOSE:
            do_mult_trans(C);
            break;
        case PAR_MODULO:
            do_mult_par_modulo(C);
            break;
        case PAR_BLOC:
            do_mult_par_bloc(C);
            break;
        default :
            perror("Internal error\n");
            assert(0);
            break;
    }

    clock_gettime(CLOCK_REALTIME, &m_time_f ) ;
    exp_time = (double)(m_time_f.tv_sec - m_time_s.tv_sec)
        + (double)(m_time_f.tv_nsec - m_time_s.tv_nsec) / 1e9 ;
    printf ( "----------> %f (ms)\n", 1e3 * exp_time ) ;
    if(verbose)
        print();
    //Do verification
    if(verify)
    {
        int err=0;
        D=malloc(sizeof(double)*sz*sz);
        do_mult(D);
        for(unsigned int i=0; i<sz*sz;i++)
        {
            if(D[i]!=C[i])
            {
                unsigned int lig=i/sz;
                unsigned int col=i%sz;
                fprintf(stderr,"Matrix differs: C[%d][%d]=%g!=D[%d][%d]=%g\n",
                        lig,col, C[i], lig, col, D[i]);
                ++err;
            }
        }
        if(err)
        {
            fprintf(stderr, "Multiplication is erroneous %d errors\n", err);
            return -1;
        }
        else
        {
            fprintf(stderr, "Multiplication is correct\n");
        }

    }
    do_free();

    return 0;
}

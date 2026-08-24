#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

// Macro for 3D indexing with halo (ghost layers)
#define INDEX(i,j,k) ((i)*(ny+2)*(nz+2) + (j)*(nz+2) + (k))

int main(int argc, char *argv[])
{
MPI_Init(&argc, &argv);

int rank;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);

// Input parameters
int px=atoi(argv[3]), py=atoi(argv[4]), pz=atoi(argv[5]);
int nx=atoi(argv[6]), ny=atoi(argv[7]), nz=atoi(argv[8]);
int T=atoi(argv[9]), seed=atoi(argv[10]), F=atoi(argv[11]);
double iso=atof(argv[12]);

// Total size including halo layers
int total=(nx+2)*(ny+2)*(nz+2);

// Allocate memory for current and next timestep
double *data=malloc(F*total*sizeof(double));
double *new_data=malloc(F*total*sizeof(double));

// Initialize data (only interior points)
srand(seed);
for(int f=0;f<F;f++)
    for(int i=1;i<=nx;i++)
        for(int j=1;j<=ny;j++)
            for(int k=1;k<=nz;k++){
                int linear=(i-1)*ny*nz+(j-1)*nz+(k-1);
                data[f*total+INDEX(i,j,k)] =
                    (double)rand()*(rank+1)/(110426.0+f+linear);
            }

// Determine process coordinates in 3D grid
int x=rank%px;
int y=(rank/px)%py;
int z=rank/(px*py);

// Identify neighboring ranks (MPI_PROC_NULL if boundary)
int east=(x<px-1)?rank+1:MPI_PROC_NULL;
int west=(x>0)?rank-1:MPI_PROC_NULL;
int north=(y>0)?rank-px:MPI_PROC_NULL;
int south=(y<py-1)?rank+px:MPI_PROC_NULL;
int front=(z<pz-1)?rank+px*py:MPI_PROC_NULL;
int back=(z>0)?rank-px*py:MPI_PROC_NULL;

// Derived datatype for YZ plane (X-direction communication)
MPI_Datatype yz_plane;
MPI_Type_vector(ny, nz, (nz+2), MPI_DOUBLE, &yz_plane);
MPI_Type_commit(&yz_plane);

// Buffers for Y and Z directions
double *sy_d=malloc(nx*nz*sizeof(double)); // send north
double *sy_u=malloc(nx*nz*sizeof(double)); // send south
double *ry_d=malloc(nx*nz*sizeof(double)); // recv from north
double *ry_u=malloc(nx*nz*sizeof(double)); // recv from south

double *sz_b=malloc(nx*ny*sizeof(double)); // send back
double *sz_f=malloc(nx*ny*sizeof(double)); // send front
double *rz_b=malloc(nx*ny*sizeof(double)); // recv from back
double *rz_f=malloc(nx*ny*sizeof(double)); // recv from front

MPI_Barrier(MPI_COMM_WORLD);
double start = MPI_Wtime();

MPI_Request reqs[12];

for(int t=0;t<T;t++)
{
    for(int f=0;f<F;f++)
    {
        double *base = &data[f*total];
        int r = 0;

        // -------- Halo Exchange: Post receives --------
        if(west!=MPI_PROC_NULL)
            MPI_Irecv(base + INDEX(0,1,1), 1, yz_plane, west, 1, MPI_COMM_WORLD, &reqs[r++]);
        if(east!=MPI_PROC_NULL)
            MPI_Irecv(base + INDEX(nx+1,1,1), 1, yz_plane, east, 0, MPI_COMM_WORLD, &reqs[r++]);

        if(north!=MPI_PROC_NULL)
            MPI_Irecv(ry_d, nx*nz, MPI_DOUBLE, north, 3, MPI_COMM_WORLD, &reqs[r++]);
        if(south!=MPI_PROC_NULL)
            MPI_Irecv(ry_u, nx*nz, MPI_DOUBLE, south, 2, MPI_COMM_WORLD, &reqs[r++]);

        if(back!=MPI_PROC_NULL)
            MPI_Irecv(rz_b, nx*ny, MPI_DOUBLE, back, 5, MPI_COMM_WORLD, &reqs[r++]);
        if(front!=MPI_PROC_NULL)
            MPI_Irecv(rz_f, nx*ny, MPI_DOUBLE, front, 4, MPI_COMM_WORLD, &reqs[r++]);

        // -------- Pack data for Y and Z directions --------
        for(int i=1;i<=nx;i++)
            for(int k=1;k<=nz;k++){
                sy_d[(i-1)*nz+(k-1)] = base[INDEX(i,1,k)];
                sy_u[(i-1)*nz+(k-1)] = base[INDEX(i,ny,k)];
            }

        for(int i=1;i<=nx;i++)
            for(int j=1;j<=ny;j++){
                sz_b[(i-1)*ny+(j-1)] = base[INDEX(i,j,1)];
                sz_f[(i-1)*ny+(j-1)] = base[INDEX(i,j,nz)];
            }

        // -------- Send halo data --------
        if(west!=MPI_PROC_NULL)
            MPI_Isend(base + INDEX(1,1,1), 1, yz_plane, west, 0, MPI_COMM_WORLD, &reqs[r++]);
        if(east!=MPI_PROC_NULL)
            MPI_Isend(base + INDEX(nx,1,1), 1, yz_plane, east, 1, MPI_COMM_WORLD, &reqs[r++]);

        if(north!=MPI_PROC_NULL)
            MPI_Isend(sy_d, nx*nz, MPI_DOUBLE, north, 2, MPI_COMM_WORLD, &reqs[r++]);
        if(south!=MPI_PROC_NULL)
            MPI_Isend(sy_u, nx*nz, MPI_DOUBLE, south, 3, MPI_COMM_WORLD, &reqs[r++]);

        if(back!=MPI_PROC_NULL)
            MPI_Isend(sz_b, nx*ny, MPI_DOUBLE, back, 4, MPI_COMM_WORLD, &reqs[r++]);
        if(front!=MPI_PROC_NULL)
            MPI_Isend(sz_f, nx*ny, MPI_DOUBLE, front, 5, MPI_COMM_WORLD, &reqs[r++]);

        MPI_Waitall(r, reqs, MPI_STATUSES_IGNORE);

        // -------- Unpack received halo data --------
        for(int i=1;i<=nx;i++)
            for(int k=1;k<=nz;k++){
                base[INDEX(i,0,k)]    = ry_d[(i-1)*nz+(k-1)];
                base[INDEX(i,ny+1,k)] = ry_u[(i-1)*nz+(k-1)];
            }

        for(int i=1;i<=nx;i++)
            for(int j=1;j<=ny;j++){
                base[INDEX(i,j,0)]    = rz_b[(i-1)*ny+(j-1)];
                base[INDEX(i,j,nz+1)] = rz_f[(i-1)*ny+(j-1)];
            }
    }

    // -------- Stencil computation (7-point) --------
    for(int f=0;f<F;f++)
        for(int i=1;i<=nx;i++)
            for(int j=1;j<=ny;j++)
                for(int k=1;k<=nz;k++)
                {
                    double sum=data[f*total+INDEX(i,j,k)];
                    int count=1;

                    // Include valid neighbors (local or halo)
                    if(i>1){ sum+=data[f*total+INDEX(i-1,j,k)]; count++; }
                    else if(west!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(0,j,k)]; count++; }

                    if(i<nx){ sum+=data[f*total+INDEX(i+1,j,k)]; count++; }
                    else if(east!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(nx+1,j,k)]; count++; }

                    if(j>1){ sum+=data[f*total+INDEX(i,j-1,k)]; count++; }
                    else if(north!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(i,0,k)]; count++; }

                    if(j<ny){ sum+=data[f*total+INDEX(i,j+1,k)]; count++; }
                    else if(south!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(i,ny+1,k)]; count++; }

                    if(k>1){ sum+=data[f*total+INDEX(i,j,k-1)]; count++; }
                    else if(back!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(i,j,0)]; count++; }

                    if(k<nz){ sum+=data[f*total+INDEX(i,j,k+1)]; count++; }
                    else if(front!=MPI_PROC_NULL){ sum+=data[f*total+INDEX(i,j,nz+1)]; count++; }

                    new_data[f*total+INDEX(i,j,k)] = sum/count;
                }

    // Swap pointers (avoid copying arrays)
    double *tmp=data;
    data=new_data;
    new_data=tmp;

    // -------- Isovalue counting --------
    long local[F];
    for(int f=0;f<F;f++) local[f]=0;

    for(int f=0;f<F;f++)
        for(int i=1;i<=nx;i++)
            for(int j=1;j<=ny;j++)
                for(int k=1;k<=nz;k++)
                {
                    double a=data[f*total+INDEX(i,j,k)];

                    // Check crossings in 3 directions
                    if(i<nx){
                        double b=data[f*total+INDEX(i+1,j,k)];
                        if((a-iso)*(b-iso)<0) local[f]++;
                    }
                    if(j<ny){
                        double b=data[f*total+INDEX(i,j+1,k)];
                        if((a-iso)*(b-iso)<0) local[f]++;
                    }
                    if(k<nz){
                        double b=data[f*total+INDEX(i,j,k+1)];
                        if((a-iso)*(b-iso)<0) local[f]++;
                    }
                }

    long global[F];

    // Sum counts across all processes
    MPI_Reduce(local,global,F,MPI_LONG,MPI_SUM,0,MPI_COMM_WORLD);

    // Root prints result
    if(rank==0){
        for(int f=0;f<F;f++) printf("%ld ",global[f]);
        printf("\n");
    }
}

double end = MPI_Wtime();

if(rank==0) printf("%f\n", end-start);

MPI_Type_free(&yz_plane);

MPI_Finalize();
return 0;


}

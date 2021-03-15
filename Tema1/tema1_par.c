#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;
pthread_barrier_t barrier;
int p;
int width, height, r, width_2, height_2;
int **result, **result_2;

#define min(a, b) ((a) < (b) ? (a) : (b))

// structura pentru un numar complex
typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia, iterations;
	double x_min, x_max, y_min, y_max, resolution;
	complex c_julia;
} params;

params par, par_2;

// citeste argumentele programului
void get_args(int argc, char **argv)
{
	if (argc < 6) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1 fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
	p = atoi(argv[5]);
}

// citeste fisierul de intrare
void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);
	fscanf(file, "%lf %lf %lf %lf",
			&par->x_min, &par->x_max, &par->y_min, &par->y_max);
	fscanf(file, "%lf", &par->resolution);
	fscanf(file, "%d", &par->iterations);

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

// scrie rezultatul in fisierul de iesire
void write_output_file(char *out_filename, int **result, int width, int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}

	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

// aloca memorie pentru rezultat
int **allocate_memory(int width, int height)
{
	int **result;
	int i;

	result = malloc(height * sizeof(int*));
	if (result == NULL) {
		printf("Eroare la malloc!\n");
		exit(1);
	}

	for (i = 0; i < height; i++) {
		result[i] = malloc(width * sizeof(int));
		if (result[i] == NULL) {
			printf("Eroare la malloc!\n");
			exit(1);
		}
	}

	return result;
}

// elibereaza memoria alocata
void free_memory(int **result, int height)
{
	int i;

	for (i = 0; i < height; i++) {
		free(result[i]);
	}
	free(result);
}

void run_julia(params *par, int **result, int width, int height, int thread_id)
{
	int w, h, i;

	int start = thread_id * (double) width / p;
	int end = min ((thread_id + 1) * (double) width / p, width);

	for (w = start; w < end; w++) {
		for (h = 0; h < height; h++) {
			int step = 0;
			complex z = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };
			
			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + par->c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + par->c_julia.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}
	pthread_barrier_wait(&barrier);

	start = thread_id * (double) (height / 2) / p;
	end = min ((thread_id + 1) * (double) (height / 2) / p, (height / 2));

	// transforma rezultatul din coordonate matematice in coordonate ecran
	for (i = start; i < end; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}

	pthread_barrier_wait(&barrier);

}

void run_mandelbrot(params *par, int **result, int width, int height, int thread_id)
{
	int w, h, i;

	int start = thread_id * (double) width / p;
	int end = min ((thread_id + 1) * (double) width / p, width);

	for (w = start; w < end; w++) {
		for (h = 0; h < height; h++) {
			complex c = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			result[h][w] = step % 256;
		}
	}

	pthread_barrier_wait(&barrier);

	start = thread_id * (double) (height/2) / p;
	end = min ((thread_id + 1) * (double) (height/2) / p, (height/2));

	// transforma rezultatul din coordonate matematice in coordonate ecran
	for (i = start; i < end; i++) {
		int *aux = result[i];
		result[i] = result[height - i - 1];
		result[height - i - 1] = aux;
	}

	pthread_barrier_wait(&barrier);
}

void* thread_function (void* args) {
	int thread_id = *(int*)args;

	// Julia:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	// - se ruleaza algoritmul
	// - se scrie rezultatul in fisierul de iesire
	// - se elibereaza memoria alocata
	if (thread_id == p-2){
		read_input_file(in_filename_julia, &par);

		width = (par.x_max - par.x_min) / par.resolution;
		height = (par.y_max - par.y_min) / par.resolution;

		result = allocate_memory(width, height);
	}
	pthread_barrier_wait(&barrier);

	run_julia(&par, result, width, height, thread_id);

	if(thread_id == p-2){
		write_output_file(out_filename_julia, result, width, height);
		free_memory(result, height);
	}
	// Mandelbrot:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	// - se ruleaza algoritmul
	// - se scrie rezultatul in fisierul de iesire
	// - se elibereaza memoria alocata

	if(thread_id == p-1){
		read_input_file(in_filename_mandelbrot, &par_2);

		width_2 = (par_2.x_max - par_2.x_min) / par_2.resolution;
		height_2 = (par_2.y_max - par_2.y_min) / par_2.resolution;

		result_2 = allocate_memory(width_2, height_2);
	}
	pthread_barrier_wait(&barrier);

	run_mandelbrot(&par_2, result_2, width_2, height_2, thread_id);

	if(thread_id == p - 1){
		write_output_file(out_filename_mandelbrot, result_2, width_2, height_2);
		free_memory(result_2, height_2);
	}
	pthread_barrier_wait(&barrier);

	pthread_exit(NULL);

} 

int main(int argc, char *argv[])
{
	int i;
	void *status;

	// se citesc argumentele programului
	get_args(argc, argv);

	pthread_t threads[p];
	int arguments[p];

	r = pthread_barrier_init(&barrier, NULL, p);
	if(r != 0)
		exit(-1);
	
	for (i = 0; i < p; i++) {
		arguments[i] = i;
		r = pthread_create(&threads[i], NULL, thread_function, &arguments[i]);

		if (r) {
			printf("Eroare la crearea thread-ului %d\n", i);
			exit(-1);
		}
	}

	for (i = 0; i < p; i++) {
		r = pthread_join(threads[i], &status);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", i);
			exit(-1);
		}
	}

	// dezalocare bariera
	r = pthread_barrier_destroy(&barrier);
	if(r != 0)
		exit(-1);
	return 0;
}
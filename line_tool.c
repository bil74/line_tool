#include <windows.h>
#include <stdio.h>

#define FILENAME_MAXLEN 260

int file_open(char *fname_i, char *open_mode_i, FILE **fhandler_o, char *errmsg_io, int errmsg_maxlen);
int fill_new_record(char *line);
long find_record(char *line);
void free_records(void);
void print_records(int o_mode, int to_sort);
int cmpfunc_sort(const void * a, const void * b);

enum mode {
	nothing = 0,
	count,
	unique,
	not_unique,
	line_end_dos,
	line_end_unix,
	line_end_mac,
};

enum line_end {
	le_dos = 0,
	le_unix = 1,
	le_mac = 2
};

struct s_line {
	char *line;
	unsigned long cnt;
};
struct s_line *lines = NULL;
long lines_cnt = 0;
char fname_i[260] = { 0 };
char fname_o[260] = { 0 };
FILE *fp_input;
FILE *fp_output;

//------------------------------------------------------------------------------------------------------------------


int main(int argc, char *argv[])
{
	int mode = count;
	int sort = 0;
	int empty = 0;
	int verbose = 0;
	const char *pb_line_prefix = "[PBTOOL - lines]";
	int desired_line_end = -1;
	
	if (argc < 2) {
		printf("%s === tool for text files ===\n", pb_line_prefix);
		printf("%s usage prog.exe [modes / switches] input_file [output_file]\n", pb_line_prefix);
		printf("%s *** WARNING: otput file will be overwritten during run!\n", pb_line_prefix);
		printf("%s modes (can't be concatenated!) [unique: -U] [not-unique: -NU] [do nothing: -N]\n", pb_line_prefix);
		printf("%s switches (can't be concatenated!) [sort: -S] [list empty lines: -E] \n", pb_line_prefix);
		printf("%s use cases below:\n", pb_line_prefix);
		printf("%s list unique lines: -U (usable switches: -S, -E)\n", pb_line_prefix);
		printf("%s list non-unique lines: -UN (usable switches: -S, -E)\n", pb_line_prefix);
		printf("%s list lines with DOS line-end: -LD (usable switches: -E)\n", pb_line_prefix);
		printf("%s list lines with UNIX line-end: -LU (usable switches: -E)\n", pb_line_prefix);
		printf("%s list lines with MAC line-end: -LM (usable switches: -E)\n", pb_line_prefix);
		printf("%s list non-empty lines: -N\n", pb_line_prefix);
		return 0;
	}
	else {	//parse params
		int i;
		int non_switch_params = 0;
		for (i = 1; i < argc; i++) {
			if (strstr(argv[i], "-U")) {
				mode = unique;
			}
			else if (strstr(argv[i], "-NU")) {
				mode = not_unique;
			}
			else if (strstr(argv[i], "-N")) {
				mode = nothing;
			}
			else if (strstr(argv[i], "-LD")) {
				mode = line_end_dos;
				desired_line_end = le_dos;
			}
			else if (strstr(argv[i], "-LU")) {
				mode = line_end_unix;
				desired_line_end = le_unix;
			}
			else if (strstr(argv[i], "-LM")) {
				mode = line_end_mac;
				desired_line_end = le_mac;
			}
			else if (strstr(argv[i], "-S")) {
				sort = 1;
			}
			else if (strstr(argv[i], "-E")) {
				empty = 1;
			}
			else if (strstr(argv[i], "-V")) {
				verbose = 1;
			}
			else {
				if (non_switch_params < 2) {
					if (non_switch_params == 0) {
						strcpy_s(fname_i, sizeof(fname_i), argv[i]);
					}
					if (non_switch_params == 1) {
						strcpy_s(fname_o, sizeof(fname_o), argv[i]);
					}
				}
				non_switch_params++;
			}
		}
	}

	//testing
	if (verbose){
		char mode_str[100];
		if (mode == count) strcpy_s(mode_str, sizeof(mode_str), "count");
		else if (mode == unique) strcpy_s(mode_str, sizeof(mode_str), "unique");
		else if (mode == not_unique) strcpy_s(mode_str, sizeof(mode_str), "not unique");
		else if (mode == nothing) strcpy_s(mode_str, sizeof(mode_str), "nothing");
		else if (mode == line_end_dos) strcpy_s(mode_str, sizeof(mode_str), "DOSline");
		else if (mode == line_end_unix) strcpy_s(mode_str, sizeof(mode_str), "UNIXline");
		else if (mode == line_end_mac) strcpy_s(mode_str, sizeof(mode_str), "MACline");
		printf("%s fname_i: %s\n", pb_line_prefix, fname_i);
		printf("%s fname_o: %s\n", pb_line_prefix, fname_o[0] != 0 ? fname_o : "(stdout)");
		printf("%s mode: %s\n", pb_line_prefix, mode_str);
		printf("%s sort: %s\n", pb_line_prefix, sort == 1 ? "yes" : "no");
		printf("%s deal with empty lines: %s\n", pb_line_prefix, empty == 1 ? "yes" : "no");
	}

	//open input and output files
	{
		char errmsgbuf[100];
		if (file_open(fname_i, "rb", &fp_input, errmsgbuf, sizeof(errmsgbuf))) {
			printf("%s *** cannot open input file '%s': %s\n", pb_line_prefix, fname_i, errmsgbuf);
			return -1;
		}

		if (fname_o[0] != 0) {
			if (file_open(fname_o, "w", &fp_output, errmsgbuf, sizeof(errmsgbuf))) {
				printf("%s *** cannot open output file '%s': %s\n", pb_line_prefix, fname_o, errmsgbuf);
				return -1;
			}
		}
		else {
			fp_output = stdout;
		}
	}

	//main task
	#define MAXLINE 10000
	{
		char input_line[MAXLINE + 1] = { 0 };
		int err_alloc;
		long i;
		while (!feof(fp_input)) {

			//read a line
			int chr;
			int current_line_end = -1;
			int chr_i = 0;	//pos in input_line
			input_line[chr_i] = 0x00;
			while((chr = fgetc(fp_input)) != EOF){
				if(chr == 0x0D){
					int chr2 = fgetc(fp_input);
					if (chr2 == 0x0A){
						current_line_end = le_dos;
					}
					else{
						current_line_end = le_mac;
						if (chr2 != EOF){
							//back file pos with one char
							fpos_t position;
							fgetpos(fp_input, &position);
							position--;
							fsetpos(fp_input, &position);
						}
					}
				}
				else if (chr == 0x0A){
					current_line_end = le_unix;
				}
				else{
					if (chr_i < MAXLINE){
						input_line[chr_i++] = chr;
						input_line[chr_i] = 0x00;
					}
					else{
						printf("%s *** line longer than %d!\n", pb_line_prefix, MAXLINE);
					}
				}
				if (current_line_end != -1){
					break;
				}

			}

			//process a line
			if (1) {
				/*
				if (input_line[strlen(input_line) - 1] == '\n')
					input_line[strlen(input_line) - 1] = 0x00;
				*/
				if (strlen(input_line) == 0 && empty == 0)
					continue;
				if (mode == nothing || desired_line_end == current_line_end) {
					printf("%s\n", input_line);
				}
				else if ((i = find_record(input_line)) >= 0) {
					lines[i].cnt++;
				}
				else {
					err_alloc = fill_new_record(input_line);
					if (err_alloc) {
						printf("%s *** malloc error! Exit all!\n", pb_line_prefix);
						break;
					}
				}
				current_line_end = -1;
			}
		}
	}
	//write output 
	if (mode != nothing)
		print_records(mode, sort);

	//closing files
	fclose(fp_input);
	if(fp_output != stdout)
		fclose(fp_output);

	free_records();
	return 0;
}

//------------------------------------------------------------------------------------------------------------------

int file_open(char *fname_i, char *open_mode_i, FILE **fhandler_o, char *errmsg_io, int errmsg_maxlen)
//returns err from fopen_s
//clears errmsg_io
{
	errno_t err;
	char errmsgbuf[100] = {0};
	int rv = 0;

	memset(errmsg_io, 0x00,  errmsg_maxlen);
	err = fopen_s(fhandler_o, fname_i, open_mode_i);
	if (err) {
		if (strerror_s(errmsgbuf, sizeof(errmsgbuf), err)) {
			strcpy_s(errmsgbuf, sizeof(errmsgbuf), "unknown error (default)");
		}
		strcpy_s(errmsg_io, errmsg_maxlen -1, errmsgbuf);
		rv = err;
	}
	return rv;
}

//------------------------------------------------------------------------------------------------------------------
int fill_new_record(char *line)
{
	int rv = 0;
	if (lines_cnt == 0) {
		lines = malloc(sizeof(struct s_line) * (lines_cnt + 1));
	}
	else {
		lines = realloc(lines, sizeof(struct s_line) * (lines_cnt + 1));
	}
	if (lines != NULL) {
		lines[lines_cnt].cnt = 1;
		lines[lines_cnt].line = malloc(strlen(line) + 1);
		if (lines[lines_cnt].line == NULL)
			rv = -1;
		else {
			strcpy_s(lines[lines_cnt].line, strlen(line) + 1, line);
			lines_cnt++;
		}
	}
	else {
		rv = -1;
	}
	return rv;
}

//------------------------------------------------------------------------------------------------------------------

long find_record(char *line)
{
	long i;
	for (i = 0; i < lines_cnt; i++) {
		if (strcmp(lines[i].line, line) == 0)
			return i;
	}
	return -1;
}

//------------------------------------------------------------------------------------------------------------------

void free_records(void)
{
	long i;
	for (i = 0; i < lines_cnt; i++) {
		if (lines[i].line)
			free(lines[i].line);
	}
	if (lines)
		free(lines);

}

//------------------------------------------------------------------------------------------------------------------

void print_records(int o_mode, int to_sort)
{
	long i;

	if (to_sort) {
		qsort(lines, lines_cnt, sizeof(struct s_line), cmpfunc_sort);
	}

	for (i = 0; i < lines_cnt; i++) {
		if (lines[i].cnt) {
			if (o_mode == count) {
				fprintf(fp_output, "%5d - ", lines[i].cnt);
				fprintf(fp_output, "%s\n", lines[i].line);
			}
			else if (o_mode == unique) {
				fprintf(fp_output, "%s\n", lines[i].line);
			}
			else if (o_mode == not_unique) {
				if (lines[i].cnt > 1) {
					fprintf(fp_output, "%s\n", lines[i].line);
				}
			}
		}

	}

}

//------------------------------------------------------------------------------------------------------------------

int cmpfunc_sort(const void * a, const void * b)
{
	/*this function belongs to qsort*/
	unsigned long cnt_a = ((struct s_line *)a)->cnt;
	unsigned long cnt_b = ((struct s_line *)b)->cnt;
	return (cnt_b - cnt_a);
}

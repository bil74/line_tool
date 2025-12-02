#include <windows.h>
#include <stdio.h>
#include <time.h>

#define FILENAME_MAXLEN 260

enum line_end {
	le_unspec = 0,
	le_dos = 1,		//0x0d, 0x0a
	le_unix = 2,	//0x0a
	le_mac = 3
};

struct s_line {
	char *line;
	unsigned long cnt;
	enum line_end le;
};

struct s_line *lines = NULL;

int file_open(char *fname_i, char *open_mode_i, FILE **fhandler_o, char *errmsg_io, int errmsg_maxlen);
int fill_new_record(char *line, enum line_end le);
long find_record(char *line);
void free_records(void);
void print_records(int i_mode, int out_order, int cnt, int line_end);
int cmpfunc_sort(const void * a, const void * b);
void rand_lines(struct s_line *lns, int lns_cnt);

enum mode {
	nothing = 0,
	group
};


enum output_order {
	semmi = 0,
	sort = 1,
	randomize = 2
};

long lines_cnt = 0;
char fname_i[260] = { 0 };
char fname_o[260] = { 0 };
FILE *fp_input;
FILE *fp_output;

//------------------------------------------------------------------------------------------------------------------


int main(int argc, char *argv[])
{
	int err_flg = 0;
	int mode = nothing;
	int out_order = semmi;	//nothing, sort, randomize
	int count = 0;
	int empty = 0;
	int verbose = 0;
	//const char *pb_line_prefix = "[PBTOOL - lines]";
	const char *pb_line_prefix = "[===]";
	int desired_line_end = le_unspec;

	srand(time(NULL));

	if (argc < 2) {
		printf("%s === tool for text files ===\n", pb_line_prefix);
		printf("%s usage prog.exe [modes / switches] input_file [output_file]\n", pb_line_prefix);
		printf("%s *** WARNING: otput file will be overwritten during run!\n", pb_line_prefix);
		printf("%s input modes [group input: -G] or just list (no grouping) \n", pb_line_prefix);
		printf("%s output switches (can't be concatenated!) [sort / randomize: -S / -R] [verbose: -V] [list empty lines: -E] [count: -C] \n", pb_line_prefix);
		printf("%s use cases below:\n", pb_line_prefix);
		printf("%s list with grouping and sort by occurence: -G -C -S \n", pb_line_prefix);
		printf("%s list lines with DOS line-end: -LD (usable switches: -E, mode must be ""nothing"")\n", pb_line_prefix);
		printf("%s list lines with UNIX line-end: -LU (usable switches: -E, mode must be ""nothing"")\n", pb_line_prefix);
		printf("%s list lines with MAC line-end: -LM (usable switches: -E, mode must be ""nothing"")\n", pb_line_prefix);
		printf("%s list non-empty lines: -N\n", pb_line_prefix);
		return 0;
	}
	else {	//parse params
		int i;
		int non_switch_params = 0;
		for (i = 1; i < argc; i++) {
			if (strstr(argv[i], "-G")) {
				mode = group;
			}
			else if (strcmp(argv[i], "-LD") == 0) {
				desired_line_end = le_dos;
			}
			else if (strcmp(argv[i], "-LU") == 0) {
				desired_line_end = le_unix;
			}
			else if (strcmp(argv[i], "-LM") == 0) {
				desired_line_end = le_mac;
			}
			else if (strcmp(argv[i], "-E") == 0) {	//deal with empty lines - non-default behavior
				empty = 1;
			}
			else if (strcmp(argv[i], "-S") == 0) {
				out_order = sort;
			}
			else if (strcmp(argv[i], "-R") == 0) {	//randomize output (can be used only if every line is unique)
				out_order = randomize;
			}
			else if (strcmp(argv[i], "-C") == 0) {	//count lines
				count = 1;
			}
			else if (strcmp(argv[i], "-V") == 0) {
				verbose = 1;
			}
			else if (strstr(argv[i], "-")) {
				printf("Unknown param %s\n", argv[i]);
				err_flg = 1;
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
		char* mode_str = "nothing";
		char* line_end_str = "not specified";
		char* count_str = "no";
		char* oo = "nothing";

		if(out_order == sort) oo = "sort";
		else if(out_order == randomize) oo = "random";

		if (count) count_str = "yes";

		if (mode == group) mode_str = "group";
		else mode_str = "list";
		
		if (desired_line_end == le_dos) line_end_str = "DOSline";
		else if (desired_line_end == le_unix) line_end_str = "UNIXline";
		else if (desired_line_end == le_mac) line_end_str = "MACline";

		printf("%s fname_i: %s\n", pb_line_prefix, fname_i);
		printf("%s fname_o: %s\n", pb_line_prefix, fname_o[0] != 0 ? fname_o : "(stdout)");
		printf("%s count: %s\n", pb_line_prefix, count_str);
		printf("%s mode: %s\n", pb_line_prefix, mode_str);
		printf("%s line end: %s\n", pb_line_prefix, line_end_str);
		printf("%s out order: %s\n", pb_line_prefix, oo);
		printf("%s deal with empty lines: %s\n", pb_line_prefix, empty == 1 ? "yes" : "no");
	}

	//open input and output files
	{
		char errmsgbuf[100];
		if (!err_flg && file_open(fname_i, "rb", &fp_input, errmsgbuf, sizeof(errmsgbuf))) {
			printf("%s *** cannot open input file '%s': %s\n", pb_line_prefix, fname_i, errmsgbuf);
			err_flg = 1;
		}

		if (!err_flg && fname_o[0] != 0) {
			if (file_open(fname_o, "w", &fp_output, errmsgbuf, sizeof(errmsgbuf))) {
				printf("%s *** cannot open output file '%s': %s\n", pb_line_prefix, fname_o, errmsgbuf);
				err_flg = 1;
			}
		}
		else {
			fp_output = stdout;
		}
	}

	if (err_flg){
		printf("%s *** error detected!\n", pb_line_prefix);
		return -1;
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
			int current_line_end = le_unspec;
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
				if (current_line_end != le_unspec){
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
				if (mode == nothing){
					err_alloc = fill_new_record(input_line, current_line_end);
					if (err_alloc) {
						printf("%s *** malloc error! Exit all #1!\n", pb_line_prefix);
						break;
					}
				}
				else if ((i = find_record(input_line)) >= 0) {
					lines[i].cnt++;
				}
				else {
					err_alloc = fill_new_record(input_line, current_line_end);
					if (err_alloc) {
						printf("%s *** malloc error! Exit all #2!\n", pb_line_prefix);
						break;
					}
				}
				current_line_end = le_unspec;
			}
		}
	}
	//write output 
	print_records(mode, out_order, count, desired_line_end);

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
int fill_new_record(char *line, enum line_end le)
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
			lines[lines_cnt].le = le;
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

void print_records(int i_mode, int out_order, int cnt, int line_end)
{
	long i;
	int print = 0;

	if (out_order == sort) {
		qsort(lines, lines_cnt, sizeof(struct s_line), cmpfunc_sort);
	}
	else if(out_order == randomize){
		rand_lines(lines, lines_cnt);
	}

	for (i = 0; i < lines_cnt; i++) {
		print = 0;
		if (lines[i].cnt) {

			//fill print
			if (i_mode == group) {
				print = 1;
			}
			else {
				if(line_end != le_unspec){
					if (line_end == lines[i].le){
						print = 1;
					}
				}
				else{
					print = 1;
				}
			}

			//count and print
			if (print){
				if (cnt) fprintf(fp_output, "%5d - ", lines[i].cnt);
				fprintf(fp_output, "%s\n", lines[i].line);
			}
		}

	}

}

//------------------------------------------------------------------------------------------------------------------
void rand_lines(struct s_line *lns, int lns_cnt)
{
	int i,r;
	struct s_line tmps;
	for(i = 0; i < lns_cnt; i++){
		r = rand()%lns_cnt;
		if (i != r){	//switch lines[i] and lines[r]
			memcpy(&tmps, &lns[i], sizeof(struct s_line));
			memcpy(&lns[i], &lns[r], sizeof(struct s_line));
			memcpy(&lns[r], &tmps, sizeof(struct s_line));
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

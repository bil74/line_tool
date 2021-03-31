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
	count = 0,
	unique = 1,
	not_unique = 2,
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
	const char *pb_line_prefix = "[PBTOOL - lines]";
	
	if (argc < 2) {
		printf("%s Listing unique, non-unique lines, or just counting occurence of lines for a text file\n", pb_line_prefix);
		printf("%s switches (can't be concatenated!) [unique: -U] [not-unique: -NU] [sort: -S] [list empty lines: -E] \n", pb_line_prefix);
		printf("%s usage prog.exe [switches] input_file [output_file]\n", pb_line_prefix);
		printf("%s *** WARNING: otput file will be overwritten during run!\n", pb_line_prefix);
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
			else if (strstr(argv[i], "-S")) {
				sort = 1;
			}
			else if (strstr(argv[i], "-E")) {
				empty = 1;
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
	{
		char mode_str[100];
		if (mode == count) strcpy_s(mode_str, sizeof(mode_str), "count");
		else if (mode == unique) strcpy_s(mode_str, sizeof(mode_str), "unique");
		else if (mode == not_unique) strcpy_s(mode_str, sizeof(mode_str), "not unique");
		printf("%s fname_i: %s\n", pb_line_prefix, fname_i);
		printf("%s fname_o: %s\n", pb_line_prefix, fname_o[0] != 0 ? fname_o : "(stdout)");
		printf("%s mode: %s\n", pb_line_prefix, mode_str);
		printf("%s sort: %s\n", pb_line_prefix, sort == 1 ? "yes" : "no");
		printf("%s deal with empty lines: %s\n", pb_line_prefix, empty == 1 ? "yes" : "no");
	}

	//open input and output files
	{
		char errmsgbuf[100];
		if (file_open(fname_i, "r", &fp_input, errmsgbuf, sizeof(errmsgbuf))) {
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
	{
		char input_sor[10000 + 1] = { 0 };
		int err_alloc;
		long i;
		while (!feof(fp_input)) {
			if (fgets(input_sor, sizeof(input_sor) - 1, fp_input)) {
				if (input_sor[strlen(input_sor) - 1] == '\n')
					input_sor[strlen(input_sor) - 1] = 0x00;
				if (strlen(input_sor) == 0 && empty == 0)
					continue;
				if ((i = find_record(input_sor)) >= 0) {
					lines[i].cnt++;
				}
				else {
					err_alloc = fill_new_record(input_sor);
					if (err_alloc) {
						printf("%s *** malloc error! Exit all!\n", pb_line_prefix);
						break;
					}
				}

			}
		}
	}
	//write output 
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

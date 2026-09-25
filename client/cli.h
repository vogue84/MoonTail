#ifndef MOONTAIL_CLI_H
#define MOONTAIL_CLI_H

void mt_config_load(void);
void mt_config_save(void);
int  mt_cmd_init(int accept_terms);
int  mt_cmd_status(void);
int  mt_cmd_prompt(const char * prompt);

int  mt_cmd_setup(int accept_terms);
int  mt_cmd_repl(void);
void mt_print_help(const char * prog);

#endif

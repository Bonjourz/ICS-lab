#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y86asm.h"
 
line_t *y86bin_listhead = NULL;   /* the head of y86 binary code line list*/
line_t *y86bin_listtail = NULL;   /* the tail of y86 binary code line list*/
int y86asm_lineno = 0; /* the current line number of y86 assemble code */

#define err_print(_s, _a ...) do { \
  if (y86asm_lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", y86asm_lineno, ## _a); \
} while (0);

int vmaddr = 0;    /* vm addr */

/* register table */
reg_t reg_table[REG_CNT] = {
    {"%eax", REG_EAX},
    {"%ecx", REG_ECX},
    {"%edx", REG_EDX},
    {"%ebx", REG_EBX},
    {"%esp", REG_ESP},
    {"%ebp", REG_EBP},
    {"%esi", REG_ESI},
    {"%edi", REG_EDI},
};

regid_t find_register(char *name)
{
	int i;
	for(i = 0; i < REG_CNT; i++){
		if(!strncmp(reg_table[i].name, name, 4))
			return reg_table[i].id;
	}
    return REG_ERR;
}

/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovl", 6,HPACK(I_RRMOVL, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVL, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVL, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVL, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVL, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVL, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVL, C_G), 2 },
    {"irmovl", 6,HPACK(I_IRMOVL, F_NONE), 6 },
    {"rmmovl", 6,HPACK(I_RMMOVL, F_NONE), 6 },
    {"mrmovl", 6,HPACK(I_MRMOVL, F_NONE), 6 },
    {"addl", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subl", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andl", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorl", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 5 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 5 },
    {"jl", 2,    HPACK(I_JMP, C_L), 5 },
    {"je", 2,    HPACK(I_JMP, C_E), 5 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 5 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 5 },
    {"jg", 2,    HPACK(I_JMP, C_G), 5 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 5 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushl", 5, HPACK(I_PUSHL, F_NONE), 2 },
    {"popl", 4,  HPACK(I_POPL, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
	int size = 32;//The size of instr_set
	int i;
	for(i = 0; i < size; i++){
		if(!strncmp(name, instr_set[i].name, instr_set[i].len))
			return &instr_set[i];
	}
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
	symbol_t *symtmp = symtab;
	/*check whether the first one is NULL*/
	if(symtmp->name == NULL)
		return NULL;
	
	else {
		do{
			if(!strncmp(name, symtmp->name, strlen(symtmp->name)))
				return symtmp;
			symtmp = symtmp->next;
		}while(symtmp != NULL);
	}
	return NULL;

}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{   
	/* check the duplicate */
	if(find_symbol(name) != NULL)
		return -1;

	/*check whether the first one if NULL*/
	if(symtab->name == NULL){
		symtab->name = name;
		symtab->addr = vmaddr;
		return 1;
	}
    /* create new symbol_t (don't forget to free it)*/
	symbol_t *new = (symbol_t *)malloc(sizeof(symbol_t));
	new->addr = vmaddr;
	new->name = name;
	new->next = NULL;

    /* add the new symbol_t to symbol table */
	new->next = symtab->next;
	symtab->next = new;
    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
void add_reloc(char *name, bin_t *bin)
{
	/*check whether the first one is NULL*/
	if(reltab->name == NULL){
		reltab->name = name;
		reltab->y86bin = bin;
		return 0;
	}

    /* create new reloc_t (don't forget to free it)*/
	reloc_t *new_reloc = (reloc_t *)malloc(sizeof(reloc_t));
	new_reloc->y86bin = bin;
	new_reloc->name = name;
	new_reloc->next = NULL;

    /* add the new reloc_t to relocation table */
	reloc_t *reloctmp = reltab;
	new_reloc->next = reloctmp->next;
	reloctmp->next = new_reloc;
}


/* macro for parsing y86 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovl')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
	char *word = *ptr;
	instr_t *instco;
    /* skip the blank */
	SKIP_BLANK(word);
	
	if(IS_END(word)) 
		return PARSE_ERR;
    
	/* find_instr and check end */
	instco = find_instr(word);
	
	if(instco == NULL) 
		return PARSE_ERR;
	
	word += instco->len;

    /* set 'ptr' and 'inst' */
	*inst = instco;
	*ptr = word;
	return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
	char *word = *ptr;
    /* skip the blank and check */
	SKIP_BLANK(word);
	if(IS_END(word))
		return PARSE_ERR;
	
	if(*word != delim){
		err_print("Invalid ','");
		return PARSE_ERR;
	}
	word ++;

    /* set 'ptr' */
	*ptr = word;
	//err_print("word:%s", word);
	//err_print("ptr:%s", *ptr);


    return PARSE_DELIM;
}

/*
 * parse_reg: parse an expected register token (e.g., '%eax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
	char *word = *ptr;
	regid_t reg;
    /* skip the blank and check */
	SKIP_BLANK(word);
	
	if(IS_END(word)) return PARSE_ERR;

    /* find register */
	reg = find_register(word);
	
	if(reg == REG_ERR){
		err_print("Invalid REG");
		return PARSE_ERR;
	}
	
	/* set 'ptr' and 'regid' */
	*regid = reg;
	*ptr = word + 4;

    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
	char *word = *ptr;
    /* skip the blank and check */
	SKIP_BLANK(word);
	
	if(IS_END(word))
		return PARSE_ERR;

    /* allocate name and copy to it */
	int len = 0;
	char *end = word;
	while(!IS_END(end) && !IS_BLANK(end)){
		if(*end == ',')
			break;

		len += 1;
		end++;
	}
	//if(IS_END(word)) return PARSE_ERR;
	char *nameco = (char *)malloc(len + 1);
	memset(nameco, '\0', len + 1);
	strncpy(nameco, word, len);

    /* set 'ptr' and 'name' */
	*ptr = end;
	*name = nameco;

    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
	char *word = *ptr;
	char **end = ptr;
	long valueco = *value;
    /* skip the blank and check */
	SKIP_BLANK(word);
	
	if(IS_END(word) || !IS_DIGIT(word)) 
		return PARSE_ERR;

	/* calculate the digit, (NOTE: see strtoll()) */
	valueco = strtoll(word, end, 0);

    /* set 'ptr' and 'value' */
	ptr = end;
	*value = valueco;

    return PARSE_DIGIT;
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
	char *word = *ptr;
	parse_t result;
	char **nameco = name;
	long *valueco = value;
    /* skip the blank and check */
	SKIP_BLANK(word);
	if(IS_END(word)) return PARSE_ERR;

    /* if IS_IMM, then parse the digit */
	if(IS_IMM(word)){
		word++;
		result = parse_digit(&word, valueco);
	}

    /* if IS_LETTER, then parse the symbol */
	else if(IS_LETTER(word)){
		result = parse_symbol(&word, nameco);
	}
	else{
		err_print("Invalid Immediate");
		return PARSE_ERR;
	}
	if(result == PARSE_ERR){ 
		err_print("Invalid Immediate");
		return result;
	}
    /* set 'ptr' and 'name' or 'value' */
	name = nameco;
	*ptr = word;
	value = valueco;
	return result;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%ebp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
	char *word = *ptr;
	*value = 0;
	regid_t *regidco = regid;
	long *valueco = value;
	parse_t result = PARSE_ERR;
    /* skip the blank and check */
	SKIP_BLANK(word);

	if(IS_END(word) || (!IS_DIGIT(word) && *word != '(')){
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
    /* calculate the digit and register, (ex: (%ebp) or 8(%ebp)) */

	if(IS_DIGIT(word)){
		result = parse_digit(&word, valueco);
		if(result == PARSE_ERR) {
			err_print("Invalid MEM");
			return result;
		}
	}

	if(*word == '('){
		word++;
		result = parse_reg(&word, regidco);
		if(result == PARSE_ERR){
			err_print("Invalid MEM");
			return result;
		}
		if(*word != ')') {
			err_print("Invalid MEM");
			return PARSE_ERR;
		}
		word++;
	}

    /* set 'ptr', 'value' and 'regid' */
	*ptr = word;
	value = valueco;
	regid = regidco;
    return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
	char *word = *ptr;
	char **nameco = name;
	long *valueco = value;
	parse_t result = PARSE_ERR;
    /* skip the blank and check */
	SKIP_BLANK(word);
	if(IS_END(word) || !IS_DIGIT(word)|| !IS_LETTER(word))
		return result;
    /* if IS_DIGIT, then parse the digit */
	if(IS_DIGIT(word)){
		result = parse_digit(&word, valueco);
		if(result == PARSE_ERR) 
			return result;
	}

    /* if IS_LETTER, then parse the symbol */
	else if(IS_LETTER(word)){
		result = parse_symbol(&word, nameco);
		if(result == PARSE_ERR) 
			return result;
	}
	else return PARSE_ERR;

    /* set 'ptr', 'name' and 'value' */
	*ptr = word;
	name = nameco;
	value = valueco;
    return result;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
	char *word = *ptr;
    /* skip the blank and check */
	SKIP_BLANK(word);
	if(IS_END(word)) 
		return PARSE_ERR;
	char *end = word;
	int len = 0;
	while(!IS_BLANK(end)){
		if(*end == ':')
			break;
		else if (IS_END(end))
			return PARSE_ERR;

		end++;
		len++;
	}

	if(*end != ':') 
		return PARSE_ERR;

    /* allocate name and copy to it */
	char *nameco = (char *)malloc(len + 1);
	memset(nameco, '\0', len + 1);
	strncpy(nameco, word, len);
	end++;

    /* set 'ptr' and 'name' */
	*name = nameco;
	*ptr = end;

    return PARSE_LABEL;
}

/*
 * parse_line: parse a line of y86 code (e.g., 'Loop: mrmovl (%ecx), %esi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y86 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y86 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{
	char *ptr = line->y86asm;
	parse_t parse_ret = PARSE_ERR;
	instr_t *instr = NULL;
	char *name;
	regid_t rA = REG_NONE;
	regid_t rB = REG_NONE;
	long value = 0;
	parse_t re = PARSE_ERR;
	long *ptrTovalue = NULL;
	int control = 1;
	int num = 0;
	int i = 0;//used in data
	int remain = 0;// used in align

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%ebp), %ecx
*           call SUM  #invoke SUM function */

	while(1){
    /* skip blank and check IS_END */
		SKIP_BLANK(ptr);
		if(IS_END(ptr))
			goto end;
    
    /* is a comment ? */
		if(IS_COMMENT(ptr)){
			if(line->type != TYPE_INS)
				line->type = TYPE_COMM;
			goto end;
		}
			

		parse_ret = parse_label(&ptr, &name);
    /* is a label ? */
		if(parse_ret == PARSE_LABEL){
			if(add_symbol(name) == -1){
				line->type = TYPE_ERR;
				err_print("Dup symbol:%s", name);
				goto end;
			}

			else{
				line->type = TYPE_INS;
				line->y86bin.addr = vmaddr;
			}
		}
		
		parse_ret = parse_instr(&ptr, &instr);
    
	/* is an instruction ? */
		if(parse_ret == PARSE_INSTR){

    /* set type and y86bin */
			line->type = TYPE_INS;
			line->y86bin.bytes = instr->bytes;
			char *bincode = (line->y86bin).codes;
			byte_t code = instr->code;//Get the instr code
			byte_t icode = HIGH(code);
			byte_t ifun = LOW(code);
			line->y86bin.addr = vmaddr;
			*bincode = code;
			bincode++;//Has set the icode and ifun
    
	/* update vmaddr */ 
			vmaddr += instr->bytes;
		
    /* parse the rest of instruction according to the itype */
			switch(icode){
				case I_HALT:
				case I_NOP:
				case I_RET:
					break;
				
				case I_RRMOVL:
					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}
					re = parse_delim(&ptr, ',');
					if(re == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}


					if(parse_reg(&ptr, &rB) == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}
					*bincode = HPACK(rA, rB);
					bincode += 1;
					break;

				case I_IRMOVL:
					re = parse_imm(&ptr, &name, &value); 
					if(re == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					if(parse_delim(&ptr, ',') == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					*bincode = HPACK(REG_NONE, rA);
					bincode++;
					if(re == PARSE_DIGIT){//digit
						/**bincode = (char)(value & 0xff);
						*(bincode + 1) = (char)((value>>8) & 0xff);
						*(bincode + 2) = (char)((value>>16)&0xff);
						*(bincode + 3) = (char)((value>>24)&0xff);*/
						ptrTovalue = bincode;
						*ptrTovalue = value;
					}
					
					else//symbol
						add_reloc(name, &line->y86bin);
					break;

				case I_RMMOVL:
					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					re = parse_delim(&ptr, ',');
					if(re == PARSE_ERR){
						//err_print("%d", 1);
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					if(parse_mem(&ptr, &value, &rB) == PARSE_ERR){
						line->type = TYPE_ERR;
						//break;
						goto end;
					}

					*bincode = HPACK(rA, rB);
					bincode++;
					*bincode = value;
					break;

				case I_MRMOVL:// mrmovl D(rB), rA
					if(parse_mem(&ptr, &value, &rB) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}

					if(parse_delim(&ptr, ',') == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;


					}
					
					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;

					}
					*bincode = HPACK(rA, rB);
					bincode++;
					*bincode = (char)(value & 0xff);
					*(bincode + 1) = (char)((value>>8) & 0xff);
					*(bincode + 2) = (char)((value >> 16) & 0xff);
					*(bincode + 3) = (char)((value >> 24) & 0xff);
					break;


				case I_ALU:
					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}
					
					if(parse_delim(&ptr, ',') == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}

					if(parse_reg(&ptr, &rB) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}

					*bincode = HPACK(rA, rB);
					bincode++;
					break;


				case I_JMP:
				case I_CALL:
					if(parse_symbol(&ptr, &name) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}
					if(IS_DIGIT(name)) {
						err_print("Invalid DEST");
						line->type = TYPE_ERR;
						goto end;
					}

					add_reloc(name, &line->y86bin);
					break;

				case I_PUSHL:
				case I_POPL:
					if(parse_reg(&ptr, &rA) == PARSE_ERR){
						line->type = TYPE_ERR;
						goto end;
					}

					*bincode = HPACK(rA, REG_NONE);
					bincode ++;
					break;

				case I_DIRECTIVE:
					bincode --;
					switch(ifun){
						case D_DATA:
							re = parse_digit(&ptr, &value);
							if(re == PARSE_ERR){
								if(parse_symbol(&ptr, &name) == PARSE_SYMBOL){
									add_reloc(name, &line->y86bin);
								}
								else{
									line->type = TYPE_ERR;
									goto end;
								}
							}
							else{
								num = line->y86bin.bytes;
								for(i = 0; i < num; i++){
									*bincode = (value & 0xff);
									bincode++;
									value = value>>8;
								}
							}
							break;

						case D_POS:
							if(parse_digit(&ptr, &value) == PARSE_ERR){
								line->type = TYPE_ERR;
								goto end;
							}

							vmaddr = value;
							line->y86bin.addr = vmaddr;
							break;

						case D_ALIGN:
							if(parse_digit(&ptr, &value) == PARSE_ERR){
								line->type = TYPE_ERR;
								goto end;
							}
							remain = vmaddr % value;
							
							if(remain != 0)
								vmaddr = (vmaddr / value) * value + value;
							line->y86bin.addr = vmaddr;
							break;

						default:
							line->type = TYPE_ERR;
							goto end;
					}
					break;

				default:
					line->type = TYPE_ERR;
					goto end;
			}
		}
	}

end:
	return line->type;
}

/*
 * assemble: assemble an y86 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y86 assembly file)
 *
 * return
 *     0: success, assmble the y86 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y86asm;

    /* read y86 code line-by-line, and parse them to generate raw y86 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        
		if ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y86 assembly code */
        y86asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y86asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        /* set defualt */
        line->type = TYPE_COMM;
        line->y86asm = y86asm;
        line->next = NULL;

        /* add to y86 binary code list */
        y86bin_listtail->next = line;
        y86bin_listtail = line;
        y86asm_lineno ++;

        /* parse */
        if (parse_line(line) == TYPE_ERR)
            return -1;
    }

    /* skip line number information in err_print() */
    y86asm_lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y86 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = reltab;
	symbol_t *stmp = NULL;
	bin_t *y86tmp = NULL;
	int addr = 0;
    
	while (rtmp != NULL && reltab->name != NULL) {
        /* find symbol */
		stmp = find_symbol(rtmp->name);
		
		if(stmp == NULL){
			err_print("Unknown symbol:'%s'", rtmp->name);
			return -1;
		}

        /* relocate y86bin according itype */
		y86tmp = rtmp->y86bin;
		addr = stmp->addr;
		char *ptr = &y86tmp->codes;
	
		switch(HIGH(y86tmp->codes[0])){
			case I_IRMOVL:
				ptr += 2;
				*ptr = addr & 0xff;
				*(ptr + 1) = (addr>>8) & 0xff;
				*(ptr + 2) = (addr>>16) & 0xff;
				*(ptr + 3) = (addr>>24) & 0xff;
				break;

			case I_JMP:
			case I_CALL:
				ptr += 1;
				*ptr = addr & 0xff;
				*(ptr + 1) = (addr>>8) & 0xff;
				*(ptr + 2) = (addr>>16) & 0xff;
				*(ptr + 3) = (addr>>24) & 0xff;
				break;

			default://case I_DIRECTIVE
				*ptr = addr & 0xff;
				*(ptr + 1) = (addr>>8) & 0xff;
				*(ptr + 2) = (addr>>16) & 0xff;
				*(ptr + 3) = (addr>>24) & 0xff;
		}

        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y86 binary file
 * args
 *     out: point to output file (an y86 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
	line_t *linetmp = y86bin_listhead;
	int addrtmp = 0;
	bin_t y86bin;
	char *word;
    /* prepare image with y86 binary code */
	while(linetmp != NULL){
		
		if(linetmp->type == TYPE_INS){
			y86bin = linetmp->y86bin;
			
			if(addrtmp < y86bin.addr){
				/*check whether is the last instruction that need space*/
				int check = 0;
				line_t *nextline = linetmp->next;
				
				while(nextline != NULL){
					if(nextline->y86bin.bytes != 0){
						check = 1;
						break;
					}
					nextline = nextline->next;
				}
				
				if(check)
					fseek(out, y86bin.addr - addrtmp, SEEK_CUR);
				addrtmp = y86bin.addr;
			}
			word = &y86bin.codes[0];
    /* binary write y86 code to output file (NOTE: see fwrite()) */
			size_t result = fwrite(word, sizeof(byte_t), y86bin.bytes, out);
			
			if(result < y86bin.bytes)
				return -1;
			
			addrtmp += y86bin.bytes;
		}
		linetmp = linetmp->next;
	}
    
    return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[26];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y86bin = &line->y86bin;
        int i;
        
        strcpy(buf, "  0x000:              | ");
        
        hexstuff(buf+4, y86bin->addr, 3);
        if (y86bin->bytes > 0)
            for (i = 0; i < y86bin->bytes; i++)
                hexstuff(buf+9+2*i, y86bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                      | ");
    }

    printf("%s%s\n", buf, line->y86asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = y86bin_listhead->next;
    
    /* line by line */
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    y86bin_listhead = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(y86bin_listhead, 0, sizeof(line_t));
    y86bin_listtail = y86bin_listhead;
    y86asm_lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = y86bin_listhead->next;
        if (y86bin_listhead->y86asm) 
            free(y86bin_listhead->y86asm);
        free(y86bin_listhead);
        y86bin_listhead = ltmp;
    } while (y86bin_listhead);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }


    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y86 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}



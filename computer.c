#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */

//R-Format
const int addu = 0x21;
const int subu = 0x23;
const int sll  = 0x00;
const int srl  = 0x02;
const int and  = 0x24;
const int or   = 0x25;
const int slt  = 0x2a;
const int jr   = 0x08;

//I-Format
const int addiu=  0x9;
const int andi =  0x8;
const int ori  =  0xd;
const int beq  =  0x4;
const int lui  =  0xf;
const int bne  =  0x5;
const int lw   = 0x23;
const int sw   = 0x2b;

//J-Format
const int j    =  0x2;
const int jal  =  0x3;

void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */

        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);


	UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
       
       
       // val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);

    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}


/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}

//POWER FUNCTION --- (GETS DECIMAL VALUE OF OPCODE FROM BINARY]
int Power (int a){
int temp =1;
if (a == 0){
	return 1;
}

else{
for(int i=a; i > 0; i--){
	 temp = 2 * temp;
}
}

	return temp;
}

//TWOS COMP METHOD FOR NEGATIVE NUMBERS
int TwosComp(int instr) { //obtained from codeforwin.org and modified it to suit my purposes
char binary[17], onesComp[17], twosComp[17];
int carry = 1;
int j = 15;
int immediate = 0;
for (int c = 15; c >= 0; c--) //fills character array with immediate value in binary
{
int k = instr >> c;
if (k & 1) {
binary[j] = '1';
}
else {
binary[j] = '0';
}
j--;
}
for (int i = 0; i < 16; i++)
{
if (binary[i] == '1')
{
onesComp[i] = '0';
}
else if (binary[i] == '0')
{
onesComp[i] = '1';
}
}
for (int i = 16; i >= 0; i--){
if (onesComp[i] == '1' && carry == 1)
{
twosComp[i] = '0';
}
else if (onesComp[i] == '0' && carry == 1)
{
twosComp[i] = '1';
carry = 0;
}
else
{
twosComp[i] = onesComp[i];
}
}
int x = 0;
for (int i = 15; i >= 0; i--){
if (twosComp[i] == '1'){
immediate += Power(j);
}
x++;
}

return 0 - immediate;
}


//GET R REGISTERS METHOD
void getRregisters(DecodedInstr* d, int instr){
int c =0;
int k = 0;
int rs = 0;
int rt = 0;
int rd = 0;
int shamt = 0;
int funct = 0;


//DECODE rs
    int i = 4;
    for (c = 25; c >= 21; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            rs += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }
    i=4;
 for (c = 20; c >= 16; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            rt += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }
    
    i=4;
 for (c = 15; c >= 11; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            rd += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    
}
   i=4;
 for (c = 10; c >= 6; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            shamt += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    
}
 i=5;
 for (c = 5; c >= 0; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            funct += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    
}
    d->regs.r.rs = rs;//RS REGISTER
    d->regs.r.rt = rt;//RT REGISTER
    d->regs.r.rd = rd;//RD REGISTER
    d->regs.r.shamt = shamt;//SHAMT REGISTER
    d->regs.r.funct = funct;//FUNCT REGISTER

}//END OF GET R REGISTERS

//GET FUNCTION FOR I REGISTERS
void getIregisters(DecodedInstr* d, int instr){
int c =0;
int k = 0;
int rs = 0;
int rt = 0;
int imm = 0;

    //DECODE rs
    int i = 4;
    for (c = 25; c >= 21; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
   
        if (k & 1) {
    
            rs += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }
    i=4;
 for (c = 20; c >= 16; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
  
            rt += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }
    i=15;
 for (c = 15; c >= 0; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
    
        if (k & 1) {
    
            imm += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }

k = instr >> 15;
    if(k & 1){
        imm = TwosComp(instr);
    }

    d->regs.i.rs = rs;//RS REGISTE
    d->regs.i.rt = rt;//RT REGISTER
    d->regs.i.addr_or_immed = imm;//IMM REGISTER
     
}//END OF GET I REGISTERS

//GET J REGISTERS METHOD
void getJregisters(DecodedInstr* d, int instr){
int c =0;
int k = 0;
int i=25;
int address = 0;
 for (c = 25; c >= 0; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes
     k = instr >> c;     
   
        if (k & 1) {
    
            address += Power(i);
            }
    else {
//do nothing because the bit is 0
    }
        i--;
    }
   address = address << 2;
    d->regs.j.target = address;

}

//DECODE OPCODE --- CALLED FROM DECODE FUNCTION
void Decode_Op(int instr, DecodedInstr* d, RegVals* rVals) { //reads first 6 bits to get the opcode
int c;
int opCode = 0;
int i = 5;
int k = 0;

//Get Opcode
for (c = 31; c >= 26; c--){//found this loop on geeksforgeeks, however I modified it to suit my purposes

    k = instr >> c;     
  
        if (k & 1) {
            opCode += Power(i);
            }
else {
//do nothing because the bit is 0
}
i--;
}

d->op = opCode;
//R-FORMAT
if (d->op == 0){
    getRregisters(d,instr);
}

//I-FORMAT
if (d->op != 0 && d->op != 2 && d->op != 3){
    getIregisters(d,instr);
}

//J-Format
if (d->op == 2 || d->op == 3){
    getJregisters(d,instr);
}


} // END OF DECODE BRACKET DONT DELETE


// printf("hello5 \n");
// //SET d->op to opCode
// d->op = opCode;
// printf("hello6 \n");
// //GET REGISTER (rs]
// d->regs.r.rs = instr << 6;
// d->regs.r.rs = instr >> 27;
// printf("hello6.1,d->regs.r.rs %d \n", d->regs.r.rs);
// //GET REGISTER (rt]
// d->regs.r.rt = instr << 11;
// d->regs.r.rt = instr >> 27;
// printf("hello6.4,d->regs.r.rt %d \n", d->regs.r.rt);
// //GET REGISTER (rd] ,
// d->regs.r.rd = instr << 16;
// d->regs.r.rd = instr >> 27;
// //GET REGISTER (shamt]
// d->regs.r.shamt = instr << 21;
// d->regs.r.shamt = instr >> 27;
// //GET REGISTER (funct]
// d->regs.r.funct = instr << 26;

// //I FORMAT IMMEDIATE
// d->regs.i.addr_or_immed = d->regs.r.rd + d->regs.r.shamt + d->regs.r.funct;
// printf("hello6.2,d->regs.r.rs %d \n", d->regs.r.rs);
// //J FORMAT TARGET
// d->regs.j.target = d->regs.r.rs + d->regs.r.rt + d->regs.r.rd + d->regs.r.shamt + d->regs.r.funct;
// printf("hello6.3,d->regs.r.rs %d \n", d->regs.r.rs);
// printf("hello7, d->op %d \n", d->op);
// printf("hello7.01,d->regs.r.rs %d \n", d->regs.r.rs);
//R-Format

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    	/* Your code goes here */	
	/* Your code goes here */
	/* Your code goes here */

	Decode_Op(instr, d,rVals);

//IF NOT R-FORMAT EXIT!!
if(d->op == 0){
    if (d->regs.r.funct != addu && d->regs.r.funct != subu && d->regs.r.funct != sll 
    && d->regs.r.funct != srl && d->regs.r.funct != and && d->regs.r.funct != or &&
     d->regs.r.funct != slt && d->regs.r.funct != jr){
        exit(1);
    }
    }
//IF NOT I OR J FORMAT EXIT!!
if (d->op !=0){
    if(d->op != addiu && d->op != andi && d->op != ori && d->op != beq && d->op != lui 
    && d->op != bne && d->op != lw && d->op != sw && d->op != j && d->op != jal){
        exit(1);
    }
}
}
/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    	/* Your code goes {{{{here */
	/* Your code goes here */
	/* Your code goes here */
			
	//R-FORMAT
    if (d->op == 0){
        if (d->regs.r.funct == addu){
            printf("addu \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        if (d->regs.r.funct == subu){
            printf("subu \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        if (d->regs.r.funct == sll){
            printf("sll \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.shamt);
        }
        if (d->regs.r.funct == srl){
            printf("srl \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.shamt);
        }
        if (d->regs.r.funct == and){
            printf("and \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        if (d->regs.r.funct == or){
            printf("or \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        if (d->regs.r.funct == slt){
            printf("slt \t$%d, $%d, $%d\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
        }
        if (d->regs.r.funct == jr){
            printf("jr \t$%d\n", d->regs.r.rs);
        }
    }
    //I-FORMAT
    if (d->op != 0 && d->op != 2 && d->op != 3){
        if (d->op == addiu){
            printf("addiu \t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        if (d->op == andi){
            printf("andi \t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        if (d->op == ori){
            printf("ori \t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        if (d->op == beq){
            printf("beq \t$%d, $%d, 0x%8.8x\n", d->regs.i.rs, d->regs.i.rt, ((d->regs.i.addr_or_immed * 4) + (mips.pc + 4)));
        }
        if (d->op == bne){
            printf("bne \t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, ((d->regs.i.addr_or_immed * 4) + (mips.pc + 4)));
        }
        if (d->op == lw){
            printf("lw \t$%d, $%d, %d\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
        if (d->op == sw){
            printf("sw \t$%d, $%d, %d\n", d->regs.i.rs, d->regs.i.addr_or_immed, d->regs.r.rt);
        }
    }
    //J-Format
if (d->op == 2 || d->op == 3){
    if(d->op == j){
        printf("j \t0x%8.8x\n", d->regs.j.target);
    }
    if(d->op == jal){
        printf("jal \t0x%8.8x\n", d->regs.j.target);
    }
}
    
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
    int val;

        //R-FORMAT
    if (d->op == 0){
        if(d->op == addu){
            val = (mips.registers[d->regs.r.rs] + mips.registers[d->regs.r.rt]);
        }
        if(d->op == subu){
            val = (mips.registers[d->regs.r.rs] - mips.registers[d->regs.r.rt]);
        }
        if(d->op == sll){
            val = (mips.registers[d->regs.r.rt] << mips.registers[d->regs.r.shamt]);
        }
        if(d->op == srl){
            val = (mips.registers[d->regs.r.rs] >> mips.registers[d->regs.r.shamt]);
        }
        if(d->op == and){
            val = (mips.registers[d->regs.r.rs] & mips.registers[d->regs.r.rt]);
        }
        if(d->op == or){
            val = (mips.registers[d->regs.r.rs] | mips.registers[d->regs.r.rt]);
        }
        if(d->op == slt){
            val = (mips.registers[d->regs.r.rs] - mips.registers[d->regs.r.rt] < 0);
        }
    }//END OF R-FORMAT

//I-FORMAT
    if (d->op != 0 && d->op != 2 && d->op != 3){
        if(d->op == addiu){
    
            val = (mips.registers[d->regs.i.rs] + d->regs.i.addr_or_immed);
            printf("imm %8.8x\n", d->regs.i.addr_or_immed);
            printf("val %8.8x\n", val);
        }
        if(d->op == andi){
            val = (mips.registers[d->regs.i.rs] & d->regs.i.addr_or_immed);
        }
        if(d->op == ori){
            val = (mips.registers[d->regs.i.rs] | d->regs.i.addr_or_immed);
        }
         if(d->op == beq){
            val = (mips.registers[d->regs.i.rs] - d->regs.i.rt);
        }       
         if(d->op == bne){
            val = (mips.registers[d->regs.i.rs] - d->regs.i.addr_or_immed);
        } 
        if(d->op == lw){
            val = (mips.registers[d->regs.i.rs] + d->regs.i.addr_or_immed);
        }
        if(d->op == sw){
            val = (mips.registers[d->regs.i.rs] + (d->regs.i.addr_or_immed * 4));
        }

    }//END I-FORMAT

    if(d->op == jal){
        return mips.pc + 4;
    }

  return val;
}
  

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    mips.pc+=4;
    	
 if(d->op ==2 || d->op == 3){
            mips.pc = d->regs.j.target;
        }
//  if(d->regs.r.funct == jr){
//             mips.pc = mips.registers[31];
//         }
 if(d->op == beq && val == 0){
     mips.pc += (d->regs.i.addr_or_immed * 4);       
        }
//     if (d->op == bne && val != 0){
//     mips.pc += (d->regs.i.addr_or_immed * 4);
// }
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    	/* Your code goes here */
	/* Your code goes here */
	/* Your code goes here */
  return 0;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    	/* Your code goes here */
	/* Your code goes here */
	/* Your code goes here */
    
    

    //R-FORMAT
    if (d->op == 0){
        *changedReg = d->regs.r.rd;
        mips.registers[d->regs.r.rd] += val;
    }//END OF R-FORMAT IF 


    if (d->op != 0 && d->op != 2 && d->op != 3){
         *changedReg = d->regs.i.rt;
        mips.registers[d->regs.i.rt] += val;
    
    }
    if(d->op == 3){
        *changedReg = 31;
        mips.registers[*changedReg] = val;
    }

    if(d->op == beq || d->op == j || d->op == jr){
        *changedReg = -1;
    }
    // else{
    //     //DO NOTHING FOR NOW
    // }


}//END OF REGWRITE






    

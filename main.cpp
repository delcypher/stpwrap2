#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include "input-tokens.h"
#include "output-tokens.h"
#include "Array.h"
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/select.h>


extern int illex();
extern FILE* ilin;
extern FILE* ilout;

extern int ollex();
extern FILE* olin;
extern FILE* olout;

using namespace std;

//STP executable
const char STP[] = "stp";

/* This is expected sequence of tokens for a (get-value () ) command
*  from illex(); after we've hit T_CHECKSAT
*
*/
namespace SMTLIBInput
{
	InputToken gvSequence[] =
	{
		T_LBRACKET,
		T_GETVALUE,
		T_LBRACKET,
		T_LBRACKET,
		T_SELECT,
		T_ARRAYID, /* (5) Array indentifier */
		T_BV,
		T_DECIMAL, /* (7) Array index in decimal */
		T_DECIMAL, /* (8) Array width in decimal (expected to be 32) */
		T_RBRACKET,
		T_RBRACKET,
		T_RBRACKET
	};
	const int GV_SEQ_SIZE=sizeof(gvSequence)/sizeof(InputToken);
	const int ARRAY_IDENTIFIER=5;
	const int ARRAY_INDEX=7;
	const int ARRAY_WIDTH=8;

	extern std::string* foundString;

	void parseInput();
}

namespace SMTLIBOutput
{
	OutputToken assertSequence[] =
	{
			T_ASSERT,
			T_LBRACKET,
			T_ARRAYID, /* (2) Array identifier */
			T_LSQRBRACKET,
			T_HEX, /* (4) Array index in hexadecimal */
			T_RSQRTBRACKET,
			T_EQUAL,
			T_HEX, /* (7) Array value */
			T_RBRACKET,
	};
	const int AS_SEQ_SIZE=sizeof(assertSequence)/sizeof(OutputToken);
	const int ARRAY_IDENTIFIER=2;
	const int ARRAY_INDEX=4;
	const int ARRAY_VALUE=7;

	extern std::string* foundString;

	void parseOutput();
	bool hex2Dec(const std::string& input, int& value, int& bitWidth);

	OutputToken finalAnswer=T_UNKNOWN; //dummy value
}

/* This is used to map array names to the Array objects in memory.
 *
 */
map<std::string,Array*> solutions;

/* We need to return answers in the order they were requested.
*  The Map structure doesn't keep track of the order items were added.
*  Even if it did that's no use we need to know the ordering of the pairs.
*/
vector< pair<string,int> > ordering;

/* File descriptors for parent->child communication via pipe.
 * pcfd[0] read end used by child.
 * pcfd[1] write end used by parent.
 */
int pcfd[2];

/* File descriptors for child->parent communication via pipe.
 * cpfd[0] read end used by parent.
 * cpfd[1] write end used by child.
 */
int cpfd[2];

void parseArgs(int argc, char* argv[]);
void setupPipes();

pid_t pid=0;


void printSMTLIBResponce();

void runSTP();

void waitForSTP();

void exitUnknown();

void cleanUpChildProcess();

int main(int argc, char* argv[])
{
	parseArgs(argc,argv);

	setupPipes();

	runSTP();

	SMTLIBInput::parseInput();

	waitForSTP();

	SMTLIBOutput::parseOutput();


	//Only print array values if we got "sat".
	if(SMTLIBOutput::finalAnswer==SMTLIBOutput::T_SAT)
		printSMTLIBResponce();

	//final cleanup
	for(map<string,Array*>::iterator i=solutions.begin(); i!= solutions.end() ; ++i)
	{
		delete i->second;
	}

	cleanUpChildProcess();

	return 0;
}

void SMTLIBInput::parseInput()
{
	/* Let illex() walk through SMTLIBv2 file
		*  and writing out to yyout until it it hits T_CHECKSAT
		*/
		if(illex() != T_CHECKSAT)
		{
			cerr << "Error: Did not find (check-sat) in input file" << endl;
			exitUnknown();
		}

		/* Now parse (get-value () ) commands until
		* we see "(exit)"
		*/
		InputToken t = static_cast<InputToken>(illex());
		int seqIndex=0; //sequence index
		string arrayName;
		int index=0;
		while(t != T_EXIT && t != T_EOF)
		{
			//Check we received expected token
			if(t != SMTLIBInput::gvSequence[seqIndex])
			{
				cerr << "Error parsing input." << endl;
				exitUnknown();
			}

			//Handle the tokens we care about
			switch(seqIndex)
			{
				case SMTLIBInput::ARRAY_IDENTIFIER:
					arrayName=*(SMTLIBInput::foundString);
					if(solutions.count(arrayName) == 0)
					{
						//We haven't recorded this array yet
						if( ! solutions.insert(pair<string,Array*>(arrayName, new Array()) ).second)
						{
							cerr << "Failed to insert Array " << arrayName << endl;
							exitUnknown();
						}
					}
					break;
				case SMTLIBInput::ARRAY_INDEX:
					if(solutions.count(arrayName) == 1)
					{
						index=atoi(SMTLIBInput::foundString->c_str());
						solutions[arrayName]->addIndex(index);
						ordering.push_back(pair<string,int>(arrayName,index));
					}
					else
					{
						cerr << "Failed to add array index " << *SMTLIBInput::foundString << " to " << arrayName << endl;
						exitUnknown();
					}
					break;

				case SMTLIBInput::ARRAY_WIDTH:
					if(solutions.count(arrayName) == 1)
					{
						solutions[arrayName]->indexBitWidth=atoi(SMTLIBInput::foundString->c_str());
					}
					else
					{
						cerr << "Failed to set width for array " << arrayName << " to " << *SMTLIBInput::foundString << endl;
						exitUnknown();
					}
					break;
			}

			t=static_cast<InputToken>(illex()); //grab next token
			seqIndex = ( ++seqIndex) % SMTLIBInput::GV_SEQ_SIZE;
		}

		//We now expect End of file.
		if(static_cast<InputToken>(illex()) != T_EOF)
		{
			cerr << "Error: Expected end of file!" << endl;
			exitUnknown();
		}

		if(fclose(ilin)==-1)
		{
			cerr << "Error closing ilin" << endl;
			exitUnknown();
		}

		//Closing write end as well! (ilout is pcfd[1])
		if(fclose(ilout) == -1)
		{
			perror("close ilout:");
			exitUnknown();
		}

		//Do clean
		if(SMTLIBInput::foundString!=0) delete SMTLIBInput::foundString;
}

void printSMTLIBResponce()
{
	int byte=0;
	int  indexBitWidth=0;
	int  valueBitWidth=0;
	Array* a;
	for(vector< pair<string,int> >::const_iterator i= ordering.begin() ; i != ordering.end() ; ++i)
	{
		a=solutions[i->first];
		byte=static_cast<int>( a->getByte(i->second) );
		indexBitWidth=a->indexBitWidth;
		valueBitWidth=a->valueBitWidth;
		cout << "(((select " << i->first << " (_ bv" << i->second << " " << indexBitWidth << " ) ) (_ bv" <<
			byte << " " << valueBitWidth << ") ))" << endl;
			
	}

}

void parseArgs(int argc, char* argv[])
{
	if(argc!=2)
	{
		cerr << "Usage:" << endl << argv[0] << " <input>" << endl << endl <<
				"<input> is a SMTLIBv2 file." << endl << endl <<
			"This is a wrapper for the executable \"" << STP << "\" in your shell's PATH." << endl;
		exit(1);
	}

	//try to open the file.
	ilin = fopen(argv[1],"r");

	if(ilin==NULL)
	{
		cerr << "Failed to open file " << argv[1] << endl;
		perror("fopen:");
		exit(1);
	}
}

void setupPipes()
{
	//Setup parent->child pipe communication
	if(pipe(pcfd) == -1)
	{
		cerr << "Couldn't set up parent->child pipe." << endl;
		perror("pipe:");
		exitUnknown();
	}

	ilout=fdopen(pcfd[1],"w");
	if(ilout==NULL)
	{
		cerr << "Couldn't set SMTLIBInput parse output to pipe.";
		perror("fdopen:");
		exitUnknown();
	}


	//Prepare child->parent pipe for communication
	if(pipe(cpfd) == -1)
	{
		cerr << "Couldn't set up child->parent pipe." << endl;
		perror("pipe:");
		exitUnknown();
	}

	olin=fdopen(cpfd[0],"r"); //The output lexer reads from the child's pipe.
	if(olin==NULL)
	{
		cerr << "Couldn't set SMTLIBOutput parse input to pipe.";
		perror("fdopen:");
		exitUnknown();
	}

	//still need to close the ends! Will do in runSTP()

}

void runSTP()
{
	fflush(stdout);
	fflush(stdin);
	pid = fork();

	if(pid==-1)
	{
		cerr << "Fork failed!" << endl;
		perror("fork:");
		exitUnknown();
	}

	if(pid == 0)
	{
		//child

		/* Close write end of parent->child.
		 * EOF will never be sent if we forget to do this!
		 */
		if(close(pcfd[1]) == -1)
		{
			perror("close (child):");
			exit(1);
		}


		//close read end of child->parent
		if(close(cpfd[0]) == -1)
		{
			perror("close (child):");
			exit(1);
		}

		/* STP supports input on standard input. We set so
		 * that the read end of the parent->child pipe is used by STP
		 * instead of Standard input
		 */
		if(dup2(pcfd[0],fileno(stdin)) == -1)
		{
			perror("dup2 (child):");
			exit(1);
		}

		/* STP's output normal goes to standard output. Instead
		 * we want the parent to receive this via the child->parent pipe.
		 */
		if(dup2(cpfd[1],fileno(stdout))== -1)
		{
			perror("dup2 (child):");
			exit(1);
		}

		//execute STP


		/* We always ask for a counter example. We do this because
		 * we don't know yet if we need to ask for values because
		 * the input SMTLIBv2 file has not been parsed.
		 */
		if(execlp(STP,STP,"--SMTLIB2", "-p", (char*) NULL)== -1)
		{
			cerr << "Failed to execute STP (" << STP << ")" << endl;
			perror("execlp:");
			exit(1);
		}


	}
	else
	{
		//parent


		//close read end of parent->child
		if(close(pcfd[0]) == -1)
		{
			perror("close (parent):");
			exitUnknown();
		}

		//close write end of child->parent
		if(close(cpfd[1])== -1)
		{
			perror("close (parent):");
			exitUnknown();
		}

		//now return. We'll wait for child once input parsing has finished

	}
}

void waitForSTP()
{
	/* Wait on the read end of the child -> parent pipe.
	 * This method of waiting for the child is more reliable than waitpid()
	 * which can cause a hang (child blocks because it can no longer write to
	 * the write end of the child -> parent pipe because the kernel's buffer
	 * is full).
	 */
	fd_set read;
	FD_ZERO(&read);
	FD_SET(cpfd[0],&read);

	int readyFd = pselect(cpfd[0] +1, &read,NULL,NULL,NULL,NULL);

	if(readyFd==1)
		return; // The fd is now ready for reading.
	else if(readyFd == -1)
	{
		perror("pselect:");
		return;
	}
}

void exitUnknown()
{
	//SMTLIBv2 response to give when something goes wrong.
	cout << "unknown" << endl;

	//Kill the child.
	if(pid!=0)
	{
		kill(pid,SIGKILL);
	}
	exit(1);
}

void SMTLIBOutput::parseOutput()
{
	OutputToken t=static_cast<OutputToken>(ollex());
	if(solutions.size() == 0)
	{
		//We don't need to get array values

		/* Because we told STP we want a counter example. We'll
		 * still see the arrays values. We'll just skip past them
		 * until we see what we want.
		 */

		while(t!= T_SAT && t!= T_UNSAT && t!= T_UNKNOWN)
		{
			t=static_cast<OutputToken>(ollex());//grab next token
			if(t==T_EOF)
			{
				cerr << "Error: STP did not respond with sat|unsat|unknown." << endl;
				exitUnknown();
			}
		}

		/* When we hit T_SAT | T_UNSAT | T_UNKNOWN token they'll be sent to
		 * stdout by flex so we don't need to do anything else.
		 */
	}
	else
	{
		//We have array values to handle.

		int seqIndex=0;
		string arrayName;
		int value=0;
		int width=0;
		int index=0;
		while(t!= T_SAT && t!= T_UNSAT && t!= T_UNKNOWN)
		{
			//Check we have expected token
			if(t != assertSequence[seqIndex])
			{
				cerr << "Error: Received unexpected token parsing STP's output." << endl;
				exitUnknown();
			}

			//handle the token
			switch(seqIndex)
			{
				case ARRAY_IDENTIFIER:
						arrayName=*foundString;

					break;

				case ARRAY_INDEX:
						if(! hex2Dec(*foundString,index,width))
						{
							cerr << "Error: Failed to convert hex (" << *foundString << ") to decimal!" << endl;
							exitUnknown();
						}
						if(solutions.count(arrayName) == 1)
							solutions[arrayName]->indexBitWidth=width;
					break;

				case ARRAY_VALUE:
						if(! hex2Dec(*foundString,value,width))
						{
							cerr << "Error: Failed to convert hex (" << *foundString << ") to decimal!" << endl;
							exitUnknown();
						}
						if(solutions.count(arrayName) == 1)
						{
							solutions[arrayName]->valueBitWidth=width;
							solutions[arrayName]->setByte(index, static_cast<unsigned char>(value));
							//cerr << "Setting: " << arrayName << "[" << index << "] = " << value << endl;
						}
					break;
			}

			//Move to next token
			seqIndex = (++seqIndex) % AS_SEQ_SIZE;
			t=static_cast<OutputToken>(ollex());
		}



	}

	//Last found token should be the answer to (check-sat)
	finalAnswer=t; //It should be T_SAT | T_UNSAT | T_UNKNOWN

	//clean up
	if(foundString!=0) delete foundString;

}

bool SMTLIBOutput::hex2Dec(const std::string& input, int& value, int& bitWidth)
{
	//Skip the "0x"
	const char* string = input.c_str() + 2;
	bitWidth= strlen(string)*4; //4-bits to every hex digit
	errno=0;
	value=strtol(string,NULL,16);
	if(errno!=0)
		return false;
	else
		return true;
}

void cleanUpChildProcess()
{
		int status;
		//Wait for child to complete
		if(waitpid(pid,&status,0)==-1)
		{
			perror("waitpid:");
			exitUnknown();
		}

		if(WIFEXITED(status) && WEXITSTATUS(status)==0)
		{
			return;
		}
		else
		{
			cerr << "STP did not return correctly!" << endl;
			exitUnknown();
		}
}

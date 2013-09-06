
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <string>
#include <stdlib.h>


std::string inputFile;
std::string outputFile;
std::string mapFile;

std::ifstream istr;
std::ifstream mstr;
std::ostream* ostr;

bool debugMode=false;
bool timestamp_mode=false;
bool convertTags=false;
int  timestampOffset=0;

const char TAG_START='[';
const char TAG_END=']';

std::string ts2hhmmss(double t)
{
  int secs = t;
  int msecs= (t-secs)*1000;

  char buf[100];
  sprintf(buf, "%02d:%02d:%02d.%02d", secs/3600, (secs/60)%60, secs%60, msecs/10);
  return buf;
}


void processFile()
{
  while (!istr.eof())
    {
      // read next line from alignment

      double start,end;
      istr >> start >> end;

      start += timestampOffset;
      end   += timestampOffset;

      // if EOF, exit otherwise output the alignment times

      if (istr.eof()) break;

      std::string startTime = ts2hhmmss(start);
      std::string endTime   = ts2hhmmss(end);
      if (timestamp_mode) *ostr << start << " " << end;
      else                *ostr << "[" << startTime << "] [" << endTime << "]";

      // skip whitespace in alignment input
      while (istr.peek()==' ') istr.get();

      //std::cout << "read line " << start << " - " << end << "\n";

      // replace each word on the current line of the alignment
      bool endLine=false;
      while (!endLine)
	{
	  // read and map next word

	  std::string word;
	  for (;;)
	    {
	      int ch = istr.get();
	      if (ch==-1) return;

              //std::cout << "char: " << ((char)ch) << "\n";

	      char c = ch;
	      if (c!=' ' && c!='\n') word += c;  // word continues
	      else
		{
		  // end of word -> write corresponding word to output

                  //std::cout << "end of word: |" << word << "|\n";

                  bool wordIsNumber = true;
                  for (int i=0;i<word.size() && wordIsNumber;i++) {
                    if (!isdigit(word[i])) wordIsNumber=false;
                  }

		  if (!word.empty() && !wordIsNumber) {
		    std::string oldWord,newWord;
		    mstr >> oldWord;

		    char newWordBuf[1000];
		    mstr.getline(newWordBuf,1000);
		    newWord = newWordBuf+1; // +1: skip first whitespace until word begins

		    if (convertTags)
		      {
			int pos;

			// remove '['
			while ((pos = newWord.find(TAG_START)) != std::string::npos) {
			  newWord.erase(pos,1);
			}

			// replace ']' with ':'

			while ((pos = newWord.find(TAG_END)) != std::string::npos) {
			  newWord[pos] = ':';
			}
		      }

		    if (debugMode)
		      { *ostr << ' ' << word << ':' << newWord; }
		    else
		      { *ostr << ' ' << newWord; }
		  }

		  if (c=='\n') { endLine=true; break; } // this was the last word on the line

		  if (c==' ') { // another word...
		    word="";
		    while (istr.peek()==' ') istr.get(); // skip whitespace
		    break;
		  }
		}
	    }
	}

      *ostr << '\n';
    }
}


void usage(const char* prgname)
{
  std::cerr << "usage: " << prgname << "\n"
	    << "  -i NAME    input filename (required)\n"
	    << "  -m NAME    input->output map file (required)\n"
	    << "  -o NAME    preprocessed text output\n"
	    << "  -d         enable debug mode\n"
	    << "  -t         timestamp output (secs)\n"
	    << "  -c         convert tags \"[bla]\" to \"bla:\"\n"
	    << "  -O SECS    timestamp offset added to the aligner output\n";
}


int main(int argc, char** argv)
{
  int opt;
  bool haveInput=false;
  bool haveOutput=false;
  bool haveMap=false;

  while ((opt=getopt(argc,argv, "hi:o:m:dtcO:")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      exit(0);
      break;

    case 'i':
      inputFile = optarg;
      haveInput = true;
      break;

    case 'o':
      outputFile = optarg;
      haveOutput = true;
      break;

    case 'm':
      mapFile = optarg;
      haveMap = true;
      break;

    case 'd':
      debugMode = true;
      break;

    case 't':
      timestamp_mode = true;
      break;

    case 'c':
      convertTags = true;
      break;

    case 'O':
      timestampOffset = atoi(optarg);
      break;
    }
  }

  if (!haveInput || !haveMap) { usage(argv[0]); exit(5); }

  istr.open(inputFile.c_str());
  mstr.open(mapFile.c_str());

  std::ofstream ofstr;
  if (!outputFile.empty()) {
    ofstr.open(outputFile.c_str());
    ostr = &ofstr;
  } else {
    ostr = &std::cout;
  }


  processFile();

  return 0;
}

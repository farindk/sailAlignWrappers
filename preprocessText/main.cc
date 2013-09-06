
#include <iostream>
#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <ctype.h>
#include <string>
#include <stdlib.h>

std::string inputFile;
std::string outputFile;
std::string mapFile;

enum ErrorCodes {
  ERR_SUCCESS,
  ERR_TAG_PARENTHESIS
};


enum State { STATE_WS, STATE_WORD, STATE_EOW, STATE_TAG, STATE_EOTAG, STATE_PUNCT };

char TAG_START = '[';
char TAG_END   = ']';


bool isws(unsigned char c)
{
  return isblank(c) || c=='\n' || c=='\r' || c==0x85;
}

bool isendofword(char c)
{
  return
    c==',' || c=='.' || c==';' || c=='?' || c=='!' || c=='"' ||
    c=='-';
}

bool isTagStart(char c) { return c=='[' || c=='('; }
bool isTagEnd(char c) { return c==']' || c==')'; }


char convert(unsigned char c)
{
  if (isalpha(c)) return toupper(c);
  if (isdigit(c)) return c; // TODO: convert numbers to strings (not so easy, because 1:n mapping)

  switch (c)
    {
      //case '-':
      //return c;

    case 0xE8:
    case 0xEA:
    case 0xE9: return 'E';

    case 0xD2:
    case 0xD3: return 'O';

    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3: return 'A';

    case 0xF9:
    case 0xFA:
    case 0xFB: return 'U';

      /*
    case 'é': return 'E';
    case 'è': return 'E';
    case 'ê': return 'E';
    case 'û': return 'U';
    case 'ú': return 'U';
    case 'ù': return 'U';
*/
    case 0x92: return '\'';
    case '`': return '\'';

    default:
      return 0;
    }
}

std::ifstream istr;
std::ostream* ostr;
std::ostream* mstr;

void printError(int errNumber, int lineNr, const char* text)
{
  std::cerr << "ERROR " << errNumber << " LINE " << lineNr << " " << text << "\n";
}


void dumpWordPair(std::string& word, std::string& origword)
{
  static bool first=true;

  while (origword[0]==' ') { origword=origword.substr(1); }

  if (first) { first=false; }
  else       { if (ostr) *ostr << ' '; }
  
  if (mstr) *mstr << word << " " << origword << "\n";
  if (ostr) *ostr << word;

  //std::cout << "|" << word << "|" << origword << "|\n";

  word="";
  origword="";
}


void processFile()
{
  std::string word;
  std::string origword;

  bool wordPresent=false;

  int nTagsOpen = 0;
  int nTags2Open = 0;
  int lineNr = 1;

  enum State state = STATE_WS;
  while (istr.eof()==false)
    {
      int c = istr.get();

      /*
      std::cout << "state=" << state
		<< " word='" << word << "'"
		<< " oldword='" << origword << "'"
                << " word.present=" << (wordPresent ? "yes" : "no")
		<< " char '" << ((char)c) << "'\n";
      */

      if (c==TAG_START) {
        nTagsOpen++;
      }
      else if (c==TAG_END) {
        nTagsOpen--;
      }
      if (c=='(') {
        nTags2Open++;
      }
      else if (c==')') {
        nTags2Open--;
      }
      else if (c=='\n') {
        lineNr++;
      }

      if (nTagsOpen < 0 || nTagsOpen > 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Tag parenthesis [ ] mismatch");
        return;
      }

      if (nTags2Open < 0 || nTags2Open > 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Tag parenthesis ( ) mismatch");
        return;
      }

      if (nTagsOpen == 1 && nTags2Open == 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Nested tag parentheses [ (");
        return;
      }

      switch (state) {
      case STATE_WS:
	if (c==TAG_START) {
	  origword += TAG_START;
	  state = STATE_TAG;
	}
	else if (!isws(c)) {
	  state = STATE_WORD;
          if (isalpha(c)) { wordPresent = true; }
	  origword += c;
	  char c2 = convert(c); if (c2) word+=c2;
	}
	break;

      case STATE_WORD:
	if (isws(c) && word.size()>0) {
          dumpWordPair(word, origword);
	  word="";
	  origword="";
	  state = STATE_WS;
          wordPresent=false;
	}
	else if (isendofword(c) && word.size()>0) {
	  origword += c;
	  state = STATE_EOW;
	}
	else if (c==TAG_START) {
	  origword += TAG_START;
	  state = STATE_TAG;
	}
	else {
	  char c2 = convert(c); if (c2) word+=c2;

	  if (c==0x0d) {
	  }
	  else if (c=='\n') {
	    origword += "\\n";
	  } else {
	    origword += c;
	  }
	}
	break;

      case STATE_EOW:
	if (isws(c)) {
          dumpWordPair(word, origword);
	  word="";
	  origword="";
	  state = STATE_WS;
          wordPresent=false;
	}
	else if (isendofword(c)) {
	  origword += c;
	}
	else if (c==TAG_START) {
	  origword += TAG_START;
	  state = STATE_TAG;
	}
	else {
          dumpWordPair(word, origword);
	  word="";
	  origword="";
          wordPresent=false;


	  char c2 = convert(c); if (c2) word+=c2;
	  origword += c;

	  state = STATE_WORD;
          if (isalpha(c)) { wordPresent = true; }
	}
	break;

      case STATE_TAG:
	if (c==TAG_END) {
	  origword += TAG_END;

          if (wordPresent) {
            dumpWordPair(word, origword);
            word="";
            origword="";
            wordPresent=false;
            state=STATE_WS;
          }
          else {
            state=STATE_EOTAG;
          }
	}
	else {
	  origword += c;
	}
	break;

      case STATE_EOTAG:
	if (c==0x0d) {
	}
	else if (isws(c)) {
	  origword += c;
	}
	else if (c==TAG_START) {
	  origword += TAG_START;
	  state = STATE_TAG;
	}
	else {
	  /*
	  if (mstr) *mstr << word << " " << origword << "\n";
	  if (ostr) *ostr << word;
	  word="";
	  origword="";
	  */

	  char c2 = convert(c); if (c2) word+=c2;
	  origword += c;

	  state = STATE_WORD;
          wordPresent=true;
	}
	break;
      }


      //std::cout << ((char)c) << " " << std::hex << c << " " << word << " " << origword << "\n";
    }

  if (ostr) *ostr << "\n";


  std::cerr << "OK\n";
}



void processFile_new()
{
  std::string word;
  std::string origword;

  int nTagsOpen = 0;
  int nTags2Open = 0;
  int lineNr = 1;

  enum State state = STATE_WS;
  while (istr.eof()==false)
    {
      int c = istr.get();
      char conv_c = convert(c);
      char c_nolf = c;
      if (c_nolf=='\n') c_nolf=' ';

      if (c==0x0D) continue;


      /*
      std::cout << "state=" << state
		<< " word='" << word << "'"
		<< " oldword='" << origword << "'"
                << " word.present=" << (wordPresent ? "yes" : "no")
		<< " char '" << ((char)c) << "'\n";
      */

      if (c==TAG_START) {
        nTagsOpen++;
      }
      else if (c==TAG_END) {
        nTagsOpen--;
      }
      if (c=='(') {
        nTags2Open++;
      }
      else if (c==')') {
        nTags2Open--;
      }
      else if (c=='\n') {
        lineNr++;
      }

      if (nTagsOpen < 0 || nTagsOpen > 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Tag parenthesis [ ] mismatch");
        return;
      }

      if (nTags2Open < 0 || nTags2Open > 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Tag parenthesis ( ) mismatch");
        return;
      }

      if (nTagsOpen == 1 && nTags2Open == 1) {
        printError(ERR_TAG_PARENTHESIS, lineNr, "Nested tag parentheses [ (");
        return;
      }



      switch (state) {
      case STATE_WS:
	if (isTagStart(c)) {
	  origword += c;
	  state = STATE_TAG;
	}
	else if (isalpha(conv_c)) {
	  state = STATE_WORD;
	  origword += c;
	  word+=conv_c;
	}
	else if (isws(c)) {
	  origword += c_nolf;
	}
	else {
	  origword += c_nolf;
	}
	break;

      case STATE_WORD:
	if (isTagStart(c)) {
          dumpWordPair(word, origword);

	  origword += c;
	  state = STATE_TAG;
	}
	else if (isalpha(conv_c)) {
	  word += conv_c;
	  origword += c;
	}
	else if (c=='\'') {
	  origword += c;
	}
	else if (isws(c)) {
          dumpWordPair(word, origword);
	  state = STATE_WS;
	}
	else {
	  state = STATE_PUNCT;
	  origword+=c_nolf;
	}
	break;

      case STATE_PUNCT:
	if (isTagStart(c)) {
          dumpWordPair(word, origword);

	  origword += c;
	  state = STATE_TAG;
	}
	else if (isalpha(conv_c)) {
          dumpWordPair(word, origword);
	  state=STATE_WORD;

	  word += conv_c;
	  origword += c;
	}
	else if (isws(c)) {
	  dumpWordPair(word, origword);
	  state=STATE_WS;
	}
	else {
	  origword += c_nolf;
	}
	break;

      case STATE_TAG:
	if (isTagEnd(c)) {
	  origword += c;
	  state=STATE_WS;
	}
	else {
	  origword += c_nolf;
	}
	break;
      }


      //std::cout << ((char)c) << " " << std::hex << c << " " << word << " " << origword << "\n";
    }

  if (ostr) *ostr << "\n";


  std::cerr << "OK\n";
}


void usage(const char* prgname)
{
  std::cerr << "usage: " << prgname << "\n"
	    << "  -i NAME    input filename (required)\n"
	    << "  -o NAME    preprocessed text output\n"
	    << "  -m NAME    input->output map file\n";
}


int main(int argc, char** argv)
{
  int opt;
  bool haveInput=false;
  bool haveOutput=false;
  bool haveMap=false;

  while ((opt=getopt(argc,argv, "hi:o:m:")) != -1) {
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
    }
  }

  if (!haveInput) { usage(argv[0]); exit(5); }

  istr.open(inputFile.c_str());

  std::ofstream ofstr;
  if (!outputFile.empty()) {
    ofstr.open(outputFile.c_str());
    ostr = &ofstr;
  } else {
    // ostr = &std::cout;
  }

  std::ofstream mfstr;
  if (!mapFile.empty()) {
    mfstr.open(mapFile.c_str());
    mstr = &mfstr;
  } else {
    // mstr = &std::cout;
  }


  processFile_new();

  return 0;
}


#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <set>
#include <limits>

#include "boost/filesystem.hpp"

std::string tmpDirPath = ".";
std::string preprocCommand = "./preprocessText/preprocessText";
std::string convertCommand = "./convertAlignment/convertAlignment";
std::string sailAlignCommand = "sail_align";
std::string sailAlignConfig = "./build-system/SailAlign-1.10/config/timit_alignment.cfg";
int processDelay = 5; // secs
int checkDelay = 5; // secs

bool debugMode = false;
bool removeTmpDir = false;
std::string wavInputFilename;
std::string txtInputFilename;
std::string outputFilename;


std::string startTime;
std::string endTime;
bool haveStartTime=false;
bool haveEndTime=false;


std::string autoLanguageConfig;
bool autoLanguage=false;


struct LanguagePack
{
  std::string language;
  std::string sailAlignConfig;
  std::set<std::string> dictionary;
  int count;
};

std::vector<LanguagePack> languages;


std::set<std::string> readDictionary(std::string dictfile)
{
  std::set<std::string> dict;

  std::ifstream istr(dictfile.c_str());
  for (;;)
    {
      char buf[1000];
      istr.getline(buf,1000);

      if (istr.eof()) {
	break;
      }

      if (buf[0]=='\\') continue;

      std::string word(buf);
      size_t p = word.find_first_of(" ('.");
      assert(p != std::string::npos);

      word = word.substr(0,p);

      for (int i=0;i<word.size();i++) {
	word[i] = toupper(word[i]);
      }

      //std::cout << word << "\n";

      dict.insert(word);
    }

  istr.close();

  return dict;
}


void readConfigFile()
{
  languages.reserve(10);

  std::ifstream istr(autoLanguageConfig.c_str());
  for (;;) {
    if (istr.peek()=='\n') { istr.ignore(1); continue; }

    if (istr.peek()=='#') {
      istr.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
      continue;
    }

    std::string lang;
    istr >> lang;

    if (istr.eof()) {
      break;
    }

    if (isupper(lang[0])) {
      std::string value;

      istr >> value;

      if (lang=="TMP_DIR_PATH") { tmpDirPath=value; }
      if (lang=="PREPROC_CMD")  { preprocCommand=value; }
      if (lang=="CONVERT_CMD")  { convertCommand=value; }
      if (lang=="SAILALIGN_CMD") { sailAlignCommand=value; }
    }
    else {
      std::string config;
      std::string dict;

      istr >> config >> dict;
      languages.push_back(LanguagePack());

      std::cerr << "reading language dictionary: " << lang << " ...\n";

      languages.back().language = lang;
      languages.back().sailAlignConfig = config;
      languages.back().dictionary = readDictionary(dict);
      languages.back().count = 0;
    }
  }
}



void autoDetectLanguage(std::string preprocTextFile)
{
  // process input file

  std::ifstream instr(preprocTextFile.c_str());
  int totalCount=0;
  for (;;)
    {
      std::string str;
      instr >> str;

      if (instr.eof()) { break; }

      for (int i=0;i<languages.size();i++) {
	if (languages[i].dictionary.find(str) != languages[i].dictionary.end()) {
	  languages[i].count++;
	}
      }

      totalCount++;
    }


  std::cerr << "probing language:\n";

  for (int i=0;i<languages.size();i++) {
    std::cerr << "  "
	      << languages[i].language << ": "
	      << languages[i].count << "/" << totalCount << "\n";
  }


  int maxFit = 0;
  for (int i=1;i<languages.size();i++) {
    if (languages[i].count > languages[maxFit].count) {
      maxFit=i;
    }
  }

  std::cout << "-> use language: " << languages[maxFit].language << "\n";

  sailAlignConfig = languages[maxFit].sailAlignConfig;
}


std::string basename(const std::string& filename)
{
  size_t pos = filename.find_last_of('/');
  if (pos==std::string::npos)
    return filename;
  else
    return filename.substr(pos+1);
}

std::string stripSuffix(const std::string& filename)
{
  size_t pos1 = filename.find_last_of('/');
  size_t pos2 = filename.find_last_of('.');
  if (pos2==std::string::npos || (pos1!=std::string::npos && pos1>pos2))
    return filename;
  else
    return filename.substr(0,pos2);
}

int ts2secs(const std::string& ts)
{
  int result=0;

  int v=0;
  for (int i=0;i<ts.size();i++) {
    char c = ts[i];
    if (c==':') {
      result *= 60;
      result += v;
      v = 0;
    } else {
      v *= 10;
      v += c-'0';
    }
  }

  return result*60 + v;
}

void postprocess(const std::string& tmpDir,
		 const std::string& labName,
		 const std::string& output)
{
  sleep(processDelay);

  std::string cmd;
  cmd = convertCommand
    + " -i " + labName
    + " -m " + tmpDir + "/map.txt"
    + " -o " + output;

  if (haveStartTime) {
    char buf[100];
    sprintf(buf,"%d",ts2secs(startTime));
    cmd = cmd + " -O " + buf;
  }

  //cmd = "cp " + labName + " " + output;
  system(cmd.c_str());
}

void runAlignment()
{
  std::string cmd;


  // create temporary directory for processing

  std::string wavInputBaseFilename = stripSuffix(basename(wavInputFilename));

  std::string tmpDirTemplate = tmpDirPath + "/alignment-" + wavInputBaseFilename + "-XXXXXX";

  char* tmpDir = new char[tmpDirTemplate.size()+1];
  strcpy(tmpDir, tmpDirTemplate.c_str());

  if (mkdtemp(tmpDir) == NULL)
    {
      perror("cannot create temporary directory");
      return;
    }


  // cut out part of the WAV file

  std::string wavFilename = wavInputFilename;

  if (haveStartTime || haveEndTime)
    {
      // generate cropped temporary WAV file

      wavFilename = std::string(tmpDir) + "/" + basename(wavInputFilename);

      cmd = "sox " + wavInputFilename + " " + wavFilename + " trim ";
      if (!haveStartTime) cmd = cmd + "0 ";
      else                cmd = cmd + startTime + " ";

      if (haveEndTime) cmd = cmd + "=" + endTime;

      system(cmd.c_str());
    }


  // copy input

  cmd = "cp " + txtInputFilename + " " + tmpDir + "/input.txt";
  system(cmd.c_str());


  // run preprocessor

  cmd = preprocCommand
    + " -i " + tmpDir + "/input.txt"
    + " -o " + tmpDir + "/preproc.txt"
    + " -m " + tmpDir + "/map.txt";
  system(cmd.c_str());


  // autodetect language

  if (autoLanguage) {
    autoDetectLanguage(std::string(tmpDir)+"/preproc.txt");
  }


  // start SailAlign process

  if (fork()==0)
    {
      cmd = sailAlignCommand
	+ " -i " + wavFilename
	+ " -c " + sailAlignConfig
	+ " -t " + tmpDir + "/preproc.txt"
	+ " -w " + tmpDir
	+ " -e " + tmpDir;
      fprintf(stderr,"running SailAlign: %s\n",cmd.c_str());
      system(cmd.c_str());
      return;
    }
  else
    {
      int nextTempID = 1;

      for (;;)
	{
	  // check for intermediate output

	  std::stringstream checkFilename;
	  checkFilename << tmpDir << "/" << wavInputBaseFilename
			<< ".iter" << nextTempID << ".lab";

	  struct stat statbuf;
	  if (stat(checkFilename.str().c_str(),&statbuf)==0)
	    {
	      std::cout << "found intermediate result " << checkFilename.str() << "\n";

	      std::stringstream tmpOutputFilename;
	      tmpOutputFilename << outputFilename << "." << nextTempID;

	      postprocess(tmpDir,
			  checkFilename.str(),
			  tmpOutputFilename.str());
	      nextTempID++;
	    }


	  // check for the final output

	  checkFilename.str("");
	  checkFilename << tmpDir << "/" << wavInputBaseFilename << ".lab";

	  if (stat(checkFilename.str().c_str(),&statbuf)==0)
	    {
	      std::cout << "found final result...\n";

	      postprocess(tmpDir,
			  checkFilename.str(),
			  outputFilename);
	      break;
	    }


	  sleep(checkDelay);
	}
    }


  // remove temporary directory

  if (removeTmpDir) {
    boost::filesystem::remove_all("tmpDir");
  }

  delete[] tmpDir;
}


void usage(const char* prgname)
{
  std::cerr << "usage: " << prgname << "\n"
	    << "  -w NAME    WAV filename (required)\n"
            << "  -s TIME    start time (mm:ss)\n"
            << "  -e TIME    end time (mm:ss)\n"
	    << "  -t NAME    transcript text filename (required)\n"
	    << "  -o NAME    preprocessed text output (required)\n"
	    << "  -a CONFIG  use automatic language detection (do not use together with -A)\n"
	    << "  -r         remove temporary directory after alignment\n"
	    << "  -d         enable debug mode\n"
	    << "  -T NAME    temp dir path\n"
	    << "  -P NAME    preprocessor command\n"
	    << "  -C NAME    convert command\n"
	    << "  -S NAME    sail align command\n"
	    << "  -A NAME    sail align config file\n";
}


int main(int argc, char** argv)
{
  int opt;
  bool haveWavInput=false;
  bool haveTxtInput=false;
  bool haveOutput=false;

  //readDictionary(argv[1]); exit(5);

  while ((opt=getopt(argc,argv, "hw:t:o:drT:P:C:S:A:s:e:a:")) != -1) {
    switch (opt) {
    case 'h':
      usage(argv[0]);
      exit(0);
      break;

    case 's':
      startTime = optarg;
      haveStartTime=true;
      break;

    case 'e':
      endTime = optarg;
      haveEndTime=true;
      break;

    case 'd':
      debugMode = true;
      break;

    case 'r':
      removeTmpDir = true;
      break;

    case 'w':
      wavInputFilename = optarg;
      haveWavInput = true;
      break;

    case 't':
      txtInputFilename = optarg;
      haveTxtInput = true;
      break;

    case 'o':
      outputFilename = optarg;
      haveOutput = true;
      break;

    case 'P':
      preprocCommand = optarg;
      break;

    case 'C':
      convertCommand = optarg;
      break;

    case 'S':
      sailAlignCommand = optarg;
      break;

    case 'A':
      sailAlignConfig = optarg;
      break;

    case 'a':
      autoLanguageConfig = optarg;
      autoLanguage = true;
      break;

    case 'T':
      tmpDirPath = optarg;
      break;
    }
  }

  if (autoLanguage) { readConfigFile(); }

  if (!haveWavInput || !haveTxtInput || !haveOutput)
    { usage(argv[0]); exit(5); }

  runAlignment();

  return 0;
}

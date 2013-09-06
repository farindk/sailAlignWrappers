sailAlignWrappers
=================

Wrappers around the sailAlign text-to-speech alignment algorithm to use it on text with punctuation.


Installation
------------

You need to have sailAlign installed.

Optionally, you may also have to install "sox" for audio file processing.
It is used for partial alignments by the runAlignment tool.


Usage
-----

The only tool you will call directly is "runAlignment".
Its options are (also shown when started without any arguments):

usage: ./runAlignment
  -w NAME    WAV filename (required)
  -s TIME    start time (mm:ss)
  -e TIME    end time (mm:ss)
  -t NAME    transcript text filename (required)
  -o NAME    preprocessed text output (required)
  -a CONFIG  use automatic language detection (do not use together with -A)
  -r         remove temporary directory after alignment
  -d         enable debug mode
  -T NAME    temp dir path
  -P NAME    preprocessor command
  -C NAME    convert command
  -S NAME    sail align command
  -A NAME    sail align config file

Only the input WAV file (-w), the transciption text (-t) and
the output text filename (-o) are required parameters.

You should be able to run the alignment simply by calling
  runAlignment -w WAV -t INPUT -o OUTPUT

The WAV file has to be mono, 16 kHz.

You can do a partial transcript by specifying the time period
with -s/-e. After everything is working fine, you may want
to add -r to tell it to remove all temporary data from disk
after an alignment has been computed.

If you are moving binaries around, you may also have to
adjust the paths to these binaries with the options -P/-C/-S.
Check the top of the program source for the defaults.

Note that alignment will take a long time. Maybe several hours.
Run first tests with small files.

While the aligner runs, it outputs intermediate alignments after
each alignment pass. The output files will be named with a sequential
number appended to the output filename. I.e. if you specify the
output to be "myoutput.txt", it will generate
  myoutput.txt.1
  myoutput.txt.2
  myoutput.txt.3
  ...
  myoutput.txt

The final output will have no number appended to it.
You should scan the output of runAlignment for strings like
"found intermediate output" to see when a pass has finished and
"found final result" as notification that the alignment is finished.

I've also uploaded two files (alignment-test-1.wav, alignment-test-1.txt),
which you can use for a test (it is a free audiobook and ebook that I got
from the web).


new configuration method
------------------------
The new way to configure the sailAlign wrapper is to write a configuration
file like "config.align". In this file, you specify the paths to the wrapper
commands and to sail_align.

Furthermore, you can define input languages. For each language, you have to
specify the sailAlign configuration file that should be used for this language
and a dictionary file.

When you specify this config file with -a to runAlignment, it will
automatically determine the language of the input text and select the
appropriate options for sailAlign.


File formats
------------

The input should be plain ASCII text, however, the preprocessor will
try to sanitize the input as much as possible.
The input may contain tags denoted by "[...]" or "(...)" which are copied over to
the output, but which are not taken into account during the alignment.

Each line in the alignment output file starts with the
start and end times of the words that follow on the line.
Like this:

[00:04:56.06] [00:04:56.07] the
[00:04:56.07] [00:04:56.34] noise
[00:04:56.34] [00:04:56.40] of
[00:04:56.40] [00:04:56.49] the
[00:04:56.49] [00:04:56.80] crowd
[00:04:56.80] [00:04:57.23] diminished
[00:04:57.23] [00:04:57.64] slightly.

There are more options in the postprocessing tool, like converting
"[abc]" tags to "abc:", or writing timestamps as seconds instead
of the hh:mm:ss format, in which case the example will look like this:

296.068865 296.078844 the
296.078844 296.348277 noise
296.348277 296.408151 of
296.408151 296.497962 the
296.497962 296.807311 crowd
296.807311 297.236408 diminished
297.236408 297.645547 slightly.

You can also add a time offset to all computed times.
These options are currently not used, but can be enabled very easily.


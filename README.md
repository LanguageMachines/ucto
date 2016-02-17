[![Build Status](https://travis-ci.org/LanguageMachines/ucto.svg?branch=master)](https://travis-ci.org/LanguageMachines/ucto) [![Language Machines Badge](http://applejack.science.ru.nl/lamabadge.php/ticcutils)](http://applejack.science.ru.nl/languagemachines/) 

================================
Ucto - A rule-based tokeniser
================================

    Centre for Language and Speech technology, Radboud University Nijmegen
    Induction of Linguistic Knowledge Research Group, Tilburg University

Website: https://languagemachines.github.io/ucto/

Ucto tokenizes text files: it separates words from punctuation, and splits
sentences. It offers several other basic preprocessing steps such as changing
case that you can all use to make your text suited for further processing such
as indexing, part-of-speech tagging, or machine translation.

Ucto comes with tokenisation rules for several languages and can be easily
extended to suit other languages. It has been incorporated for tokenizing Dutch
text in Frog (https://languagemachines.github.io/frog), our Dutch
morpho-syntactic processor.

Features:

- Comes with tokenization rules for English, Dutch, French, Italian, Turkish,
  Spanish, Portuguese and Swedish; easily extendible to other languages.
- Recognizes dates, times, units, currencies, abbreviations.
- Recognizes paired quote spans, sentences, and paragraphs.
- Produces UTF8 encoding and NFC output normalization, optionally accepting
  other input encodings as well.
- Optional conversion to all lowercase or uppercase.
- Supports FoLiA XML (https://proycon.github.io/folia)

Ucto was written by Maarten van Gompel and Ko van der Sloot. Work on Ucto was
funded by NWO, the Netherlands Organisation for Scientific Research, under the
Implicit Linguistics project and the CLARIN-NL program.

------------------------------------------------------------

To install ucto, first consult whether your distribution's package manager has an up-to-date package for TiMBL.
If not, for easy installation of ucto and all dependencies, it is included as part of our software
distribution LaMachine: https://proycon.github.io/LaMachine .

To compile and install manually from source, provided you have all the
dependencies installed:

    $ bash bootstrap.sh
    $ ./configure
    $ make
    $ sudo make install

You will need current versions of the following dependencies of our software:

* [ticcutils](https://github.com/LanguageMachine/ticcutils) - A shared utility library
* [libfolia](https://github.com/LanguageMachines/libfolia)  - A library for the FoLiA format.

As well as the following 3rd party dependencies:

* ``icu`` - A C++ library for Unicode and Globalization support. On Debian/Ubuntu systems, install the package libicu-dev.
* ``libxml2`` - An XML library. On Debian/Ubuntu systems install the package libxml2-dev.
* A sane build environment with a C++ compiler (e.g. gcc or clang), autotools, libtool, pkg-config

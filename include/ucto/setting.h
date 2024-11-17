/*
  Copyright (c) 2006 - 2024
  CLST - Radboud University
  ILK  - Tilburg University

  This file is part of Ucto

  Ucto is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  Ucto is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/ucto/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/

#ifndef UCTO_SETTING_H
#define UCTO_SETTING_H

namespace TiCC {
  class LogStream;
  class UnicodeRegexMatcher;
  class UniFilter;
}

namespace Tokenizer {

  using namespace icu;

  extern const std::string config_prefix();

  class Rule {
    friend std::ostream& operator<< (std::ostream&, const Rule& );
  public:
  Rule(): regexp(0){
    };
    Rule( const UnicodeString& id, const UnicodeString& pattern);
    ~Rule();
    UnicodeString id;
    UnicodeString pattern;
    TiCC::UnicodeRegexMatcher *regexp;
    bool matchAll( const UnicodeString&,
		   UnicodeString&,
		   UnicodeString&,
		   std::vector<UnicodeString>& );
  private:
    Rule( const Rule& ) = delete; // inhibit copies
    Rule& operator=( const Rule& ) = delete; // inhibit copies
  };


  class Quoting {
    friend std::ostream& operator<<( std::ostream&, const Quoting& );
    struct QuotePair {
      UnicodeString openQuote;
      UnicodeString closeQuote;
    };
  public:
    void add( const UnicodeString&, const UnicodeString& );
    UnicodeString lookupOpen( const UnicodeString &) const;
    UnicodeString lookupClose( const UnicodeString & ) const;
    bool empty() const { return _quotes.empty(); };
    bool emptyStack() const { return quotestack.empty(); };
    void clearStack() { quoteindexstack.clear(); quotestack.clear(); };
    int lookup( const UnicodeString&, int& );
    void eraseAtPos( int pos ) {
      quotestack.erase( quotestack.begin()+pos );
      quoteindexstack.erase( quoteindexstack.begin()+pos );
    }
    void flushStack( int ); //renamed from eraseBeforeIndex
    void push( int i, UChar32 c ){
      quoteindexstack.push_back(i);
      quotestack.push_back(c);
    }
  private:
    std::vector<QuotePair> _quotes;
    std::vector<int> quoteindexstack;
    std::vector<UChar32> quotestack;
  };

  class Setting {
  public:
    ~Setting();
    bool read( const std::string&,
	       const std::string&,
	       int,
	       TiCC::LogStream*,
	       TiCC::LogStream* );
    bool read_rules( const std::string& );
    bool read_filters( const std::string& );
    bool read_quotes( const std::string& );
    bool read_eosmarkers( const std::string& );
    bool read_abbreviations( const std::string&,  UnicodeString& );
    void add_rule( const UnicodeString&,
		   const std::vector<UnicodeString>& );
    void sort_rules( std::map<UnicodeString, Rule *>&,
		     const std::vector<UnicodeString>& );
    static std::set<std::string> installed_languages();
    UnicodeString eosmarkers;
    std::map<UnicodeString, UnicodeString> macros;
    std::vector<Rule *> rules;
    std::map<UnicodeString, Rule *> rulesmap;
    std::map<UnicodeString, int> rules_index;
    std::string splitter;
    Quoting quotes;
    TiCC::UniFilter filter;
    std::string set_file; // the name of the settingsfile
    std::string version;  // the version of the datafile
    int tokDebug;
    TiCC::LogStream *theErrLog;
    TiCC::LogStream *theDbgLog;
  };

} // namespace Tokenizer

#endif

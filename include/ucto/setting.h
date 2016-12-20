/*
  Copyright (c) 2006 - 2017
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
}

namespace Tokenizer {

  class Rule {
    friend std::ostream& operator<< (std::ostream&, const Rule& );
  public:
  Rule(): regexp(0){
    };
    Rule( const UnicodeString& id, const UnicodeString& pattern);
    ~Rule();
    UnicodeString id;
    UnicodeString pattern;
    UnicodeRegexMatcher *regexp;
    bool matchAll( const UnicodeString&,
		   UnicodeString&,
		   UnicodeString&,
		   std::vector<UnicodeString>& );
  private:
    Rule( const Rule& ); // inhibit copies
    Rule& operator=( const Rule& ); // inhibit copies
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
    bool read( const std::string&, int, TiCC::LogStream* );
    bool readrules( const std::string& );
    bool readfilters( const std::string& );
    bool readquotes( const std::string& );
    bool readeosmarkers( const std::string& );
    bool readabbreviations( const std::string&,  UnicodeString& );
    void add_rule( const UnicodeString&, const std::vector<UnicodeString>& );
    void sortRules( std::map<UnicodeString, Rule *>&,
		    const std::vector<UnicodeString>& );
    UnicodeString eosmarkers;
    std::vector<Rule *> rules;
    std::map<UnicodeString, Rule *> rulesmap;
    std::map<UnicodeString, int> rules_index;
    Quoting quotes;
    UnicodeFilter filter;
    std::string set_file; // the name of the settingsfile
    std::string version;  // the version of the datafile
    int tokDebug;
    TiCC::LogStream *theErrLog;
  };

} // namespace Tokenizer

#endif

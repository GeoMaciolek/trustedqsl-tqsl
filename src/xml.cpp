/***************************************************************************
                          xml.cpp  -  description
                             -------------------
    begin                : Fri Aug 9 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "xml.h"
#include "expat.h"
#include <stack>
#include <fstream>
#include <zlib.h>

using namespace std;

namespace tqsllib {

pair<string,bool>
XMLElement::getAttribute(const string& key) {
	string s;
	XMLElementAttributeList::iterator pos;
	pos = _attributes.find(key);
	pair<string,bool> rval;
	if (pos == _attributes.end())
		rval.second = false;
	else {
		rval.first = pos->second;
		rval.second = true;
	}
	return rval;
}

void
XMLElement::xml_start(void *data, const XML_Char *name, const XML_Char **atts) {
	XMLElement *el = (XMLElement *)data;
	XMLElement new_el(name);
//cout << "Element: " << name << endl;
	for (int i = 0; atts[i]; i += 2) {
		new_el.setAttribute(atts[i], atts[i+1]);
	}
	if (el->_parsingStack.empty()) {
		el->_parsingStack.push_back(el->addElement(new_el));
//cout << "Empty: " << new_el.getElementName() << endl;
	} else {
//cout << "Adding: " << el->_parsingStack.back()->second.getElementName() << endl;
		new_el.setPretext(el->_parsingStack.back()->second.getText());
		el->_parsingStack.back()->second.setText("");
		el->_parsingStack.push_back(el->_parsingStack.back()->second.addElement(new_el));
	}
}

void
XMLElement::xml_end(void *data, const XML_Char *name) {
	XMLElement *el = (XMLElement *)data;
	if (!(el->_parsingStack.empty()))
		el->_parsingStack.pop_back();
}

void
XMLElement::xml_text(void *data, const XML_Char *text, int len) {
	XMLElement *el = (XMLElement *)data;
	el->_parsingStack.back()->second._text.append(text, len);
}

/*
bool
XMLElement::parseFile(const char *filename) {
	ifstream in;

	in.open(filename);
	if (!in)
		return false;	// Failed to open file
	char buf[256];
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, (void *)this);
	XML_SetStartElementHandler(xp, &XMLElement::xml_start);
	XML_SetEndElementHandler(xp, &XMLElement::xml_end);
	XML_SetCharacterDataHandler(xp, &XMLElement::xml_text);

	_parsingStack.clear();	
	while (in.get(buf, sizeof buf, 0).good()) {
		// Process the XML
		if (XML_Parse(xp, buf, strlen(buf), 0) == 0) {
			XML_ParserFree(xp);
			return false;
		}
	}

	bool rval = !in.bad();
	if (!rval)
		rval = (XML_Parse(xp, "", 0, 1) != 0);
	XML_ParserFree(xp);
	return rval;
}
*/

bool
XMLElement::parseFile(const char *filename) {
	gzFile in = gzopen(filename, "rb");

	if (!in)
		return false;	// Failed to open file
	char buf[256];
	XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, (void *)this);
	XML_SetStartElementHandler(xp, &XMLElement::xml_start);
	XML_SetEndElementHandler(xp, &XMLElement::xml_end);
	XML_SetCharacterDataHandler(xp, &XMLElement::xml_text);

	_parsingStack.clear();
	int rcount;
	while ((rcount = gzread(in, buf, sizeof buf)) > 0) {
		// Process the XML
		if (XML_Parse(xp, buf, rcount, 0) == 0) {
			gzclose(in);
			XML_ParserFree(xp);
			return false;
		}
	}
	gzclose(in);
	bool rval = (rcount == 0);
	if (rval)
		rval = (XML_Parse(xp, "", 0, 1) != 0);
	XML_ParserFree(xp);
	return rval;
}


static struct {
	char c;
	const char *ent;
} xml_entity_table[] = {
	{ '"', "&quot;" },
	{ '\'', "&apos;" },
	{ '>', "&gt;" },
	{ '<', "&lt;" }
};

static string
xml_entities(const string& s) {
	string ns = s;
	string::size_type idx = 0;
	while ((idx = ns.find('&', idx)) != string::npos) {
		ns.replace(idx, 1, "&amp;");
		idx++;
	}
	for (int i = 0; i < int(sizeof xml_entity_table / sizeof xml_entity_table[0]); i++) {
		while ((idx = ns.find(xml_entity_table[i].c)) != string::npos)
			ns.replace(idx, 1, xml_entity_table[i].ent);
	}
	return ns;
}

/* Stream out an XMLElement as XML text */
ostream&
operator<< (ostream& stream, XMLElement& el) {
	bool ok;
	XMLElement subel;
 	if (el.getElementName() != "") {
	 	stream << "<" << el.getElementName();
 		string key, val;
	 	bool ok = el.getFirstAttribute(key, val);
 		while (ok) {
 			stream << " " << key << "=\"" << xml_entities(val) << "\"";
	 		ok = el.getNextAttribute(key, val);
 		}
	 	if (el.getText() == "" && !el.getFirstElement(subel)) {
 			stream << " />";
 			return stream;
	 	} else
			stream << ">";
	}
	ok = el.getFirstElement(subel);
	while (ok) {
		string s = subel.getPretext();
		if (s != "")
			stream << xml_entities(s);
		stream << subel;
		ok = el.getNextElement(subel);
	}
	if (el.getText() != "")
		stream << xml_entities(el.getText());
	if (el.getElementName() != "")
		stream << "</" << el.getElementName() << ">";
	return stream;
}

}	// namespace tqsllib

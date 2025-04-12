#include "ParcerHTML.h"

#include <vector>
#include <string>
#include <iostream>
#include <iterator>
#include <regex>
#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include "gumbo.h"

#define STRINGIZE(x) #x
#define TO_STRING(x) STRINGIZE(x)

//ParcerHTML::ParcerHTML(std::vector<std::string> HTML_strings) {
ParcerHTML::ParcerHTML(std::string HTML_strings, std::string SourceHost) {
	if (HTML_strings.length() == 0) { throw std::domain_error(std::string(__FILE__) + ": no strings in input vector: " + std::string(TO_STRING(HTML_strings))); }

	GumboOutput* output = gumbo_parse(HTML_strings.c_str());
	findLinks(output->root, SourceHost);

	gumbo_destroy_output(&kGumboDefaultOptions, output);

	// Удаление html информации в <>
	std::regex tagRegex(R"((<[^>]*>|<!--[^>]*-->))");
	Line = std::regex_replace(HTML_strings, tagRegex, " ");

	// Удаление строки вида &nbsp;
	std::regex pattern_nbsp(R"((&nbsp;\s*)+)");
	Line = std::regex_replace(Line, pattern_nbsp, " ");

	// Удалим все символы, которые не буквы и не цифры
	std::regex pattern_keep_alphanumeric(R"([^
 a b c d e f g h i j k l m n o p q r s t u v w x y z
 а б в г д е ё ж з и й к л м н о п р с т у ф х ц ч ш щ ъ ы ь э ю я
 А Б В Г Д Е Ё Ж З И Й К Л М Н О П Р С Т У Ф Х Ц Ч Ш Щ Ъ Ы Ь Э Ю Я
 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z])");
	// Не могу обяснить, почему не работает регулярное выражение
	//std::regex pattern_keep_alphanumeric(R"([^a-zA-Zа-яА-Я0-9])");

	Line = std::regex_replace(Line, pattern_keep_alphanumeric, " ");

	// Переведем в нижний регистр
	boost::locale::generator gen;
	std::locale loc = gen(""); // Используем локаль по умолчанию
	Line = boost::locale::to_lower(Line, loc);

	// Удаление лишних пробелов
	std::regex SPACEpattern(R"(\s+)");
	Line = std::regex_replace(Line, SPACEpattern, "_");

	// Заполним набор для хранения частот
	unsigned int lineLength = Line.length();
	unsigned int cut_end_pos{ 0 };
	unsigned int cut_start_pos{ 0 };
	for (unsigned int iter = 0; iter < lineLength; ++iter) {
		if (Line[iter] == '_') {
			cut_end_pos = iter;
			std::string word = Line.substr(cut_start_pos, cut_end_pos - cut_start_pos);
			if ((word.length() >= 3) && (word.length() <= 40)) {
				Frequencies[word]++;
			}
			cut_start_pos = iter + 1;
		}
		else if (iter == (lineLength - 1)) {
			cut_end_pos = lineLength;
			std::string word = Line.substr(cut_start_pos, cut_end_pos - cut_start_pos);
			if ((word.length() >= 3) && (word.length() <= 40)) {
				Frequencies[word]++;
			}
		}
	}
}

std::set<std::string> ParcerHTML::getLinks() {
	return Links;
}


std::string ParcerHTML::getLine() {
	return Line;
}

std::vector<std::string> ParcerHTML::getWords() {
	return Words;
}

std::map<std::string, int> ParcerHTML::getFrequencies() {
	return Frequencies;
}

void ParcerHTML::findLinks(GumboNode* node, const std::string& SourceHost) {
	if (node->type == GUMBO_NODE_ELEMENT) {
		GumboAttribute* href;
		if ((href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
			std::string link = href->value;
			if (!link.empty()) {

				// Переведем в нижний регистр
				//boost::locale::generator gen;
				//std::locale loc = gen(""); // Используем локаль по умолчанию
				//link = boost::locale::to_lower(link, loc);

				//Сссылки внутри сайта начинаются не с "http://", а с "/" или с просто текста ссылки,
				 //поэтому нужно внутренние сслыки дополнить полным адресом
				std::string const http_pref = "http://";
				std::string const https_pref = "https://";

				bool isHTTP = false;
				bool isHTTPS = false;
				if ((link.length() >= http_pref.length() &&
					link.compare(0, http_pref.length(), http_pref) == 0)) {
					isHTTP = true;
				}
				if ((link.length() >= https_pref.length() &&
					link.compare(0, https_pref.length(), https_pref) == 0)) {
					isHTTPS = true;
				}
				if ((isHTTP == false) && (isHTTPS == false)) {
					if (link[0] == '/') {
						link = SourceHost + link;
					}
					else {
						link = SourceHost + "/" + link;
					}
				}
				//std::regex pattern_http(http_pref);
				//link = std::regex_replace(link, pattern_http, "");
				std::regex pattern_slash(R"(/$)");
				link = std::regex_replace(link, pattern_slash, "");

				// Регулярное выражение для проверки расширений файлов
				std::regex fileExtensionRegex(R"(\.(pdf|djvu|jpeg|jpg|doc|tiff|png|xls|css|zip|tar|7zip|bmp)$)");
				// Проверяем расширение файла с помощью регулярного выражения
				if (!std::regex_search(link, fileExtensionRegex)) {
					// Если расширение соответствует указанным, добавим link
					Links.insert(link);
				}
			}
		}
	}
	if (node->type == GUMBO_NODE_ELEMENT || node->type == GUMBO_NODE_DOCUMENT) {
		GumboVector* children = &node->v.element.children;
		for (unsigned int i = 0; i < children->length; ++i) {
			findLinks(static_cast<GumboNode*>(children->data[i]), SourceHost);
		}
	}
}
#pragma once
#include <vector>
#include <string>
#include <map>
#include <set>
#include "gumbo.h"

class ParcerHTML {
private:
	std::string Line; // Большая строка со всеми словами, разделенная "_"
	std::vector<std::string> Words; // Вектор всех слов юольще 3х символов
	std::set<std::string> Links; // Сопоставление для хранения ссылок, найденных на странице
	std::map<std::string, int> Frequencies; // Набор для хранения частот отдельных слов (пара: слово, количество в векторе)
	void findLinks(GumboNode* node, const std::string& SourceLink);
public:
	//ParcerHTML(std::vector<std::string> HTML_strings);
	ParcerHTML(std::string HTML_strings, std::string SourceHost);
	std::set<std::string> getLinks();
	std::string getLine();
	std::vector<std::string> getWords();
	std::map<std::string, int> getFrequencies();
};
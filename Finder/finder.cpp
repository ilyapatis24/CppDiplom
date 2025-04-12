#include <vector>
#include <string>
#include <iostream>
#include <iterator>
#include <regex>
#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include "../Spider/database.h"

// Функция для сортировки ссылок по весам
std::vector<std::string> findByFrequency(std::map<std::string, int>& linkWeight) {
	std::vector<std::string> seachResults;

	// Создаем вектор пар (link, frequency) из map
	std::vector<std::pair<std::string, int>> vec(linkWeight.begin(), linkWeight.end());

	// Сортируем по убыванию frequency
	std::sort(vec.begin(), vec.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.second > rhs.second;
		});

	// Заполняем seachResults отсортированными значениями link
	for (const auto& pair : vec) {
		seachResults.push_back(pair.first);
	}

	return seachResults;
}

std::vector<std::string> finder(std::string inSeachString, database& DB) {
	std::vector<std::string> seachResults;

	// Удалим все символы, которые не буквы и не цифры
	try
	{
		std::regex pattern_keep_alphanumeric(R"([^
 a b c d e f g h i j k l m n o p q r s t u v w x y z
 а б в г д е ё ж з и й к л м н о п р с т у ф х ц ч ш щ ъ ы ь э ю я
 А Б В Г Д Е Ё Ж З И Й К Л М Н О П Р С Т У Ф Х Ц Ч Ш Щ Ъ Ы Ь Э Ю Я
 A B C D E F G H I J K L M N O P Q R S T U V W X Y Z])");
		inSeachString = std::regex_replace(inSeachString, pattern_keep_alphanumeric, " ");

		// Удаление лишних пробелов
		std::regex SPACEpattern(R"(\s+)");
		inSeachString = std::regex_replace(inSeachString, SPACEpattern, "_");
	}
	catch (const std::exception& ex) {
		std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
		std::cout << "Regular expression error: ";
		std::string except = ex.what();
		std::cout << "\n" << except;
	}

	// Переведем в нижний регистр
	boost::locale::generator gen;
	std::locale loc = gen(""); // спользуем локаль по умолчанию
	inSeachString = boost::locale::to_lower(inSeachString, loc);

	// Заполним set для хранения слов поиска
	std::set<std::string> setInWords;
	unsigned int cut_end_pos{ 0 };
	unsigned int cut_start_pos{ 0 };
	unsigned int stringLength = inSeachString.length();
	for (unsigned int iter = 0; iter < stringLength; ++iter) {
		if (inSeachString[iter] == '_') {
			cut_end_pos = iter;
			std::string word = inSeachString.substr(cut_start_pos, cut_end_pos - cut_start_pos);
			if (word.length() >= 3) {
				setInWords.insert(word);
			}
			cut_start_pos = iter + 1;
		}
		else if (iter == (stringLength - 1)) {
			cut_end_pos = stringLength;
			std::string word = inSeachString.substr(cut_start_pos, cut_end_pos - cut_start_pos);
			if (word.length() >= 3) {
				setInWords.insert(word);
			}
		}
	}
	std::cout << "\nСлова для поиска: \n";
	for (const auto& word : setInWords) {
		std::cout << word << std::endl;
	}
	// Хранение результатов запроса для каждого слова (ссылка, частота)
	std::vector <std::map<std::string, int>> resultsPerWord;

	// Вектор для хранения слов запроса, порядок слов соответсвует resultsPerWord
	// Это проще, чем городить ещё один set
	std::vector<std::string> wordsInOrder;

	// Набор для хранения удельных весов каждой ссылки
	std::map<std::string, int> linkWeight;

	try {
		for (const auto& word : setInWords)
		{
			try {
				resultsPerWord.push_back(DB.seachRequest(word));
				wordsInOrder.push_back(word);
			}
			catch (const std::exception& ex) {
				std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
				std::cout << "\n Try to find <" + word << "> in database: ";
				std::string except = ex.what();
				std::cout << "\n" << except;
			}
		}
	}
	catch (const std::exception& ex) {
		std::cout << "Try to connect to database\n";
		std::string except = ex.what();
		std::cout << "\n" << except;
	}
	std::cout << "\nРезультаты поиска для слов: \n";
	unsigned int wordIter = 0;
	for (const auto& vectors : resultsPerWord) {
		std::cout << wordsInOrder[wordIter] << ": \n";
		for (const auto& pair : vectors) {
			std::cout << pair.first << ": ";
			std::cout << pair.second << std::endl;
			//seachResults.push_back(pair.first);

			// Заполним набор ссылок с весами
			linkWeight[pair.first] += pair.second;
		}
		++wordIter;
	}
	std::cout << std::endl;
	std::cout << "\nСсылки с весами (в порядке убывания веса)\n";

	// Вызываем функцию для сортировки linkWeight по убыванию frequency
	std::vector<std::string> sortedLinks = findByFrequency(linkWeight);

	// Отображаем отсортированные ссылки
	for (const auto& link : sortedLinks) {
		std::cout << link << ": " << linkWeight[link] << std::endl;
	}
	seachResults = std::move(sortedLinks);
	return seachResults;
}
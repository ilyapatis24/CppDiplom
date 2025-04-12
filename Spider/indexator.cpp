#include "indexator.h"
#include <iostream>
#include <string.h>
#include <set>
#include <map>
#include <exception>
#include "database.h"
#include <regex>
#include <iterator>
#include <vector>
#include "HTTPclient.h"
#include "ParcerHTML.h"


std::set<std::string> indexator(database& DB, std::string inLink) {

	std::set<std::string> Links; // Набор ссылок, найденных на странице
	std::map<std::string, int> Frequencies; // Частоты слов, найденных на странице
	int link_id; // Идентификатор страницы, которую индексируем
	std::map<std::string, int> WordIdPair; // Идентификаторы и соответсвующие слова в таблице
	std::set<std::string> wordsInDB;		// Слова, полученные из базы данных
	std::set<std::string> wordsInPage;	    // Слова, наденные на странице (для проверки наличия их в базе данных)
	std::string host;						// Адрес хоста
	std::string target;						// Ресурс на хосте
	bool isHTTPS = false;					// Поддерживает ли хост https

	// Функция возвращает кортеж: (адрес индексируемой страницы, set новых ссылок, набор: (слово, частота))
	std::tuple <std::string, std::set<std::string>, std::map<std::string, int>> indexatorResult;

	//Определим тип сервера: http или https
	const std::string http_pref = "http://";
	const std::string https_pref = "https://";
	// отрезать часть URL после #
	auto grid_ptr = inLink.find('#');
	if (grid_ptr != std::string::npos)
	{
		inLink = inLink.substr(0, grid_ptr);
	}
	if (inLink.compare(0, https_pref.length(), https_pref) == 0) {
		isHTTPS = true;
		//std::regex pattern_https(https_pref);
		//host = std::regex_replace(inLink, pattern_https, "");
	}
	else if (inLink.compare(0, http_pref.length(), http_pref) == 0) {
		isHTTPS = false;
		//std::regex pattern_http(http_pref);
		//host = std::regex_replace(inLink, pattern_http, "");
	}
	else {
		host = inLink;
	}
	std::regex pattern_https(https_pref);
	std::regex pattern_http(http_pref);
	host = (isHTTPS) ? std::regex_replace(inLink, pattern_https, "") : std::regex_replace(inLink, pattern_http, "");

	// Разделим адрес на host и target
	size_t slashPos = host.find("/");
	if (slashPos != std::string::npos) {
		std::string temp_str = host;
		host = host.substr(0, slashPos);

		// Найдем символ "?" в temp_str
		size_t questionMarkPos = temp_str.find("?");
		if (questionMarkPos != std::string::npos) {
			// Если "?" найден, обрежем строку до этого символа
			temp_str = temp_str.substr(0, questionMarkPos);
		}

		target = temp_str.substr(slashPos);
	}
	else {
		host = host;
		target = "/";
	}

	try {
		HTTPclient client; // Клиент для скачивания страницы
		std::string response = ""; // Строка с ответом

		if (isHTTPS)
		{
			client.performGetRequest(host, "443", target, 11);
		}
		else
		{
			client.performGetRequest(host, "80", target, 11);
		}
		response = client.getData();
		try
		{
			// Пробуем парсить страницу
			if (isHTTPS)
			{
				host = https_pref + host;
			}
			else {
				host = http_pref + host;
			}
			ParcerHTML parcerHTML(response, host);
			Links = parcerHTML.getLinks();
			Frequencies = parcerHTML.getFrequencies();

			for (const auto& line : Links) {
				//std::cout << line << std::endl;
			}

			try
			{
				// Добавим в базу адресс страницы, которую проиндексировали (если такого адреса там ещё нет)
				DB.link_add(inLink); // Просто бросим исключения, если адресс уже проиндексирован

				// Вносим в базу данных частоты слов для определенной страницы
				// Получим id страницы, которую индексируем
				try {
					link_id = DB.getLinkId(inLink);

					// Получим таблицу слов с id целиком
					try {
						WordIdPair = DB.getWordId();
						bool isNewWordAdd = false;
						// Добавим новые слова, если такие есть
						for (const auto& pair : Frequencies) {
							std::string wordInPage = pair.first;
							if (WordIdPair[wordInPage] == 0) {
								// Добавим в базу данных новое слово
								isNewWordAdd = true;
								try {
									DB.word_add(wordInPage);
								}
								catch (const std::exception& ex) {
									std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
									std::cout << "\n Fail to add new word in database: ";
									std::string except = ex.what();
									std::cout << "\n" << except;
									DB.CloseConnection();
									return Links;
								}
							}
						}

						// Если были добавления слов, то загрузим заново таблицу из  базы данных
						if (isNewWordAdd) {
							try {
								WordIdPair = DB.getWordId();
							}
							catch (const std::exception& ex) {
								std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
								std::cout << "\n Fail to get words ID from database: ";
								std::string except = ex.what();
								std::cout << "\n" << except;
								DB.CloseConnection();
								return Links;
							}
						}

						//Заполним таблицу частот, если успешно получили ID страницы
						for (const auto& pair : Frequencies) {
							//std::cout << counter << ". " << pair.first << ": " << pair.second << std::endl;
							//++counter;
							int wordFrequency = pair.second;
							int wordId = WordIdPair[pair.first];
							try {
								DB.frequency_add(link_id, wordId, wordFrequency);
							}
							catch (const std::exception& ex) {
								std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
								std::cout << "\nTry to add frequency for word " << pair.first << ": ";
								std::string except = ex.what();
								std::cout << "\n" << except;
								DB.CloseConnection();
								return Links;
							}
						}
					}
					catch (const std::exception& ex) {
						std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
						std::cout << "\n Fail to get words ID from database: ";
						std::string except = ex.what();
						std::cout << "\n" << except;
						DB.CloseConnection();
						return Links;
					}

				}
				catch (const std::exception& ex) {
					std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
					std::cout << "\n Fail to get address ID from database " + inLink << ": ";
					std::string except = ex.what();
					std::cout << "\n" << except;
					DB.CloseConnection();
					return Links;
				}


			}
			catch (const std::exception& ex) {
				std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
				std::cout << "\n Fail to add new address " << inLink << ": ";
				std::string except = ex.what();
				std::cout << "\n" << except;
				DB.CloseConnection();
				return Links;
			}
			indexatorResult = std::make_tuple(inLink, Links, Frequencies);
		}
		catch (const std::exception& ex) {
			std::cout << "\n Fail to parce page " + inLink << ": ";
			std::string except = ex.what();
			std::cout << "\n" << except;
			return Links;
		}
	}
	catch (const std::exception& ex) {
		std::cout << __FILE__ << ", line: " << __LINE__ << std::endl;
		std::cout << "\n Fail to load page: ";
		std::string except = ex.what();
		std::cout << "\n" << except;
		return Links;

	}
	//std::cout << "\nИндексатор отработал и не упал" << std::endl;
	return Links;
};
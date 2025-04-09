#pragma once

#include <mutex>
#include <pqxx/pqxx>
#include <windows.h>
#include <set>

class database {
private:
	std::unique_ptr <pqxx::connection> c;
	std::mutex mtx; // добавляем мьютекс
	std::string str_creation = {
			"CREATE TABLE IF NOT EXISTS links ("
			"id SERIAL PRIMARY KEY, "
			"link TEXT UNIQUE NOT NULL); "
			"CREATE TABLE IF NOT EXISTS words ("
			"id SERIAL PRIMARY KEY, "
			"word VARCHAR(40) UNIQUE NOT NULL); "
			"CREATE TABLE IF NOT EXISTS frequencies ("
			"links_id INTEGER REFERENCES links(id), "
			"words_id INTEGER REFERENCES words(id), "
			"frequency INTEGER, "
			"CONSTRAINT pk PRIMARY KEY(links_id, words_id));"
	};
	std::string str_delete = {
			"DROP TABLE IF EXISTS frequencies; "
			"DROP TABLE IF EXISTS words; "
			"DROP TABLE IF EXISTS links;"
	};

public:
	database();

	void SetConnection(std::string DataBaseHostName,
		std::string DataBaseName,
		std::string DataBaseUserName,
		std::string DataBasePassword,
		int DataBasePort);

	void table_create();

	void table_delete();

	void CloseConnection();

	database(const database&) = delete; // Запретим копирование

	database& operator=(const database&) = delete; // Запретим копирование

	void word_add(const std::string newWord);

	void link_add(const std::string newLink);

	std::map < std::string, int> getWordId();

	int getLinkId(const std::string& linkValue);

	void frequency_add(const int linkID, const int wordID, const int frequency);

	std::map<std::string, int> seachRequest(std::string word_to_search);
};
#pragma once

#include <string>
#include <fstream>
#include <map>

class ParcerINI {
private:
	std::map<std::string, std::string> Sections;  //Section <Section_name, <var_name,var_value>>
	std::string section_finder(std::string str_for_find);
	std::string var_finder(std::string str_for_find);
	std::string var_value_finder(std::string str_for_find);

public:
	ParcerINI(std::string file_path);
	template<typename T>
	T get_value(const std::string section_var);
};
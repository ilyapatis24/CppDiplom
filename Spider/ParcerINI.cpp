#include "ParcerINI.h"

std::string ParcerINI::section_finder(std::string str_for_find) {
	int b_pos{ 0 }, e_pos{ 0 };
	b_pos = str_for_find.find('[');
	int com_pos = str_for_find.find(';');
	if (b_pos < 0) { return ""; }
	if (com_pos == 0) return "";
	e_pos = str_for_find.find(']');
	if (e_pos < 0) { return""; }
	return str_for_find.substr(b_pos + 1, e_pos - b_pos - 1);
}
std::string ParcerINI::var_finder(std::string str_for_find) {
	int com_pos = str_for_find.find(';');
	if (com_pos == 0) return "";
	int equal_pos = str_for_find.find('=');
	if (equal_pos < 0) { return""; }
	int b_pos{ 0 };
	std::string::iterator IT = str_for_find.begin();
	while ((*IT == ' ') || (*IT == '    ')) {
		IT++; b_pos++;
	}
	int symbol_num{ 0 };
	while ((*IT != ' ') && (*IT != '    ') && (*IT != '=')) {
		IT++;
		symbol_num++;
		if (*IT == ';') return "";
	}
	return str_for_find.substr(b_pos, symbol_num);
}
std::string ParcerINI::var_value_finder(std::string str_for_find) {
	int com_pos = str_for_find.find(';');
	if (com_pos == 0) return "";
	int equal_pos = str_for_find.find('=');
	if (equal_pos < 0) { return ""; }
	if (equal_pos == str_for_find.length() - 1) { return ""; }
	int b_pos = equal_pos + 1;
	std::string::iterator IT = str_for_find.begin() + b_pos;
	while ((*IT == ' ') || (*IT == '    ')) { IT++; b_pos++; }
	int symbol_num{ 1 };

	while (IT < str_for_find.end() - 1) {
		if ((*IT == ' ') || (*IT == '    ') || (*IT == ';')) {
			symbol_num--;
			break;
		}
		IT++;
		symbol_num++;
	}
	return str_for_find.substr(b_pos, symbol_num);
}
ParcerINI::ParcerINI(std::string file_path) {
	std::ifstream fin(file_path);
	if (!fin.is_open()) { throw std::invalid_argument("file is not exists"); }
	std::string line;
	std::string section = "";
	std::string var = "";
	std::string var_value = "";
	int line_number{ 0 };
	while (std::getline(fin, line)) {
		std::string tmp_str = "";
		tmp_str = section_finder(line);
		if (tmp_str != "") {
			section = tmp_str;
		}
		tmp_str = var_finder(line);
		if (tmp_str != "") {
			var = tmp_str;
		}
		tmp_str = var_value_finder(line);
		if ((tmp_str != "") && (var != "") && (section != "")) {
			var_value = tmp_str;
			Sections[section + "." + var] = var_value;
		}
		line_number++;
	}
	if (section == "") { throw std::domain_error("no sections in file: " + file_path); }
	if (var == "" || var_value == "") { throw std::domain_error("no variables defined in file: " + file_path); }
	fin.close();
}
template<>
int ParcerINI::get_value(const std::string section_var) {
	auto answer = Sections[section_var];
	if (answer == "") { throw std::invalid_argument("no such pair section + var:" + section_var); }
	return std::stoi(Sections[section_var]);
}
template<>
double ParcerINI::get_value(const std::string section_var) {
	auto answer = Sections[section_var];
	if (answer == "") { throw std::invalid_argument("no such pair section + var:" + section_var); }
	return std::stod(answer);
}
template<>
std::string ParcerINI::get_value(const std::string section_var) {
	auto answer = Sections[section_var];
	if (answer == "") { throw std::invalid_argument("no such pair section+var: " + section_var); }
	return Sections[section_var];
}

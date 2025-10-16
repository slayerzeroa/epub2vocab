#pragma once
#include <filesystem>
#include <string>

// exe 옆의 vocab.txt를 받아서 py/lemmatize_list.py 실행 → vocab_lemma.txt 생성
// return: 프로세스 종료코드(0=성공)
int connect_dictionary(std::string word);
void save_to_file(const std::string& path, const std::string& content);
std::string print_definition(const std::string &word);
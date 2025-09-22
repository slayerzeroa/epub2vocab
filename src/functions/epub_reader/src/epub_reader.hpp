#pragma once
#include <string>

// epub 파일의 본문 전체 텍스트를 반환
// 오류 시 예외(std::runtime_error) 발생
std::string extract_epub_text(const std::string& epub_path);

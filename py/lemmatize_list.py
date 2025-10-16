# #!/usr/bin/env python3
# import sys, re
# from pathlib import Path
# import nltk
# from nltk.corpus import wordnet as wn
# from nltk.stem import WordNetLemmatizer

# LEM_ORDER = [wn.VERB, wn.NOUN, wn.ADJ, wn.ADV]  # 맥락 없을 때 우선순위

# def normalize(s: str) -> str:
#     # 아포스트로피/대시 정규화 + 소문자
#     return (s.replace("’","'").replace("‘","'").replace("–","-").replace("—","-")
#               .strip().lower())

# def lemma_of(token: str) -> str:
#     # 1) WordNet morphy (POS 없이) 먼저 시도
#     m = wn.morphy(token)
#     if m: return m
#     # 2) POS 별로 차례대로 시도해서 처음 성공하는 것 채택
#     wnl = WordNetLemmatizer()
#     for pos in LEM_ORDER:
#         got = wnl.lemmatize(token, pos=pos)
#         if got != token:
#             return got
#     return token

# def read_words(path: Path):
#     # 파일에서 단어 줄단위 읽기(숫자/헤더/공백 라인 제거)
#     lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
#     words = []
#     for ln in lines:
#         ln = ln.strip()
#         if not ln: continue
#         if re.match(r"^unique\b", ln, re.I):  # "Unique ..." 헤더 스킵
#             continue
#         if re.fullmatch(r"[A-Za-z][A-Za-z'-]*", ln):
#             words.append(normalize(ln))
#     return words

# def main():
#     if len(sys.argv) < 2:
#         print("Usage: python lemmatize_list.py <word_list.txt> [--map]", file=sys.stderr)
#         sys.exit(1)

#     path = Path(sys.argv[1])
#     show_map = ("--map" in sys.argv)

#     words = read_words(path)
#     lemmas = {}
#     for w in words:
#         l = lemma_of(w)
#         # 같은 lemma로 여러 변형이 매핑될 수 있으니 대표만 유지
#         if l not in lemmas: lemmas[l] = set()
#         lemmas[l].add(w)

#     if show_map:
#         # 원형과 그에 매핑된 원 단어들(정렬) 출력
#         for l in sorted(lemmas):
#             variants = ", ".join(sorted(lemmas[l]))
#             print(f"{l}\t<- {variants}")
#     else:
#         # 원형만 유니크+정렬
#         for l in sorted(lemmas):
#             print(l)

# if __name__ == "__main__":
#     main()


#!/usr/bin/env python3
import sys, re
from pathlib import Path
import argparse
import nltk
from nltk.corpus import wordnet as wn
from nltk.stem import WordNetLemmatizer
from wordfreq import zipf_frequency

LEM_ORDER = [wn.VERB, wn.NOUN, wn.ADJ, wn.ADV]  # 맥락 없을 때 우선순위

def normalize(s: str) -> str:
    # 아포스트로피/대시 정규화 + 소문자
    return (s.replace("’","'").replace("‘","'").replace("–","-").replace("—","-")
              .strip().lower())

def lemma_of(token: str) -> str:
    # 1) WordNet morphy (POS 없이) 먼저 시도
    m = wn.morphy(token)
    if m: return m
    # 2) POS 순차 시도
    wnl = WordNetLemmatizer()
    for pos in LEM_ORDER:
        got = wnl.lemmatize(token, pos=pos)
        if got != token:
            return got
    return token

def read_words(path: Path):
    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    words = []
    for ln in lines:
        ln = ln.strip()
        if not ln: continue
        if re.match(r"^unique\b", ln, re.I):  # "Unique ..." 헤더 스킵
            continue
        if re.fullmatch(r"[A-Za-z][A-Za-z'-]*", ln):
            words.append(normalize(ln))
    return words

def filter_by_zipf(words, max_zipf=4.5, lang="en"):
    """
    Zipf 빈도 기준으로 '쉬운 단어' 제외.
    - max_zipf: 이 값 이상이면 제외 (기본 4.0은 상위 약 10k~20k 단어 수준)
    - lang: 언어 코드 ('en', 'ko', 'es' 등)
    """
    filtered = []
    for w in words:
        # zipf_frequency(word, language) → log10 빈도 스코어
        freq = zipf_frequency(w.lower(), lang)
        if freq < max_zipf:
            filtered.append(w)
    return filtered


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("infile", help="줄당 1단어 파일 (C++ 결과)")
    ap.add_argument("--map", action="store_true", help="원형 ← 변형들 매핑 형태로 출력")
    ap.add_argument("--out", default="", help="출력 파일 경로(미지정시 stdout)")
    args = ap.parse_args()

    in_path = Path(args.infile)
    if not in_path.exists():
        print(f"[error] not found: {in_path}", file=sys.stderr)
        return 2

    words = read_words(in_path)
    lemmas = {}
    for w in words:
        l = lemma_of(w)
        lemmas.setdefault(l, set()).add(w)

    # 결과 문자열 구성
    if args.map:
        lines = [f"{l}\t<- {', '.join(sorted(lemmas[l]))}" for l in sorted(lemmas)]
        lines = filter_by_zipf(lines)
        text = "\n".join(lines) + "\n"
    else:
        uniq = sorted(lemmas)  # 키만 정렬
        uniq = filter_by_zipf(uniq)
        header = f"{len(uniq)}"
        text = header + "\n" + "\n".join(uniq) + "\n"

    # 저장 또는 stdout
    if args.out:
        out_path = Path(args.out)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")
        print(f"[ok] saved → {out_path}")
    else:
        sys.stdout.write(text)

    return 0

if __name__ == "__main__":
    sys.exit(main())

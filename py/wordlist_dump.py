# pip install nltk
# import nltk
# from nltk.corpus import stopwords

# # 처음 한 번만 다운로드 필요
# nltk.download('stopwords')

# # 영어 불용어 리스트 가져오기
# stops = stopwords.words('english')

# # 파일로 저장
# with open("./stopwords.txt", "w", encoding="utf-8") as f:
#     for w in sorted(set(stops)):
#         f.write(w + "\n")

# print(f"Saved {len(stops)} stopwords to stopwords.txt")


# import nltk
# for pkg in ["wordnet","omw-1.4","averaged_perceptron_tagger"]:
#     nltk.download(pkg)
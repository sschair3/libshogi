# ENGINE6

engine5 (PST + KP、すべて差分計算) と同じ探索・評価関数で、
**落ち着いた局面だけで学習した表**を使う

- PST: learn/pst_kp_quiet.txt
- KP : learn/kp_quiet.bin

# 表の作り方

1. make_quiet/quiet で、落ち着いた局面 (王手されていない、駒の取り合いで得をする手がない) だけを書き出す
   → learn/data_quiet/01.txt 〜 10.txt
2. `uv run python convert_np.py data_quiet data_quiet`
3. `uv run python train_kp.py data_quiet _quiet`

# 結果

- engine1 (駒割りだけ) と深さ 4 で 60 局: 33 勝 25 敗 2 分 (engine5 は 15 勝 43 敗 2 分)

# 時間の使い方 (floodgate 向けに調整)

- 思考時間 = min(残り時間 ÷ 25 + 加算 × 0.9 (秒読みなら 秒読み − 余裕), 残り時間 + 秒読み − 余裕)
  - フィッシャーの加算は指した後に足されるので、この手では使える時間に数えない
  - 余裕は USI オプション NetworkDelay (既定 1200 ミリ秒)
- 思考時間いっぱいまで反復深化を続け、時間になったら途中で止める。
  途中で止まった深さでも、読み終わった手の中により良い手があればその手を指す

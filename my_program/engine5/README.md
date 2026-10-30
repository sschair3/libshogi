# ENGINE5

engine4 (PST の差分計算) に KP 特徴量 (玉の位置 × 駒 × マス) を足した評価関数

- PST: learn/pst_kp.txt (train_kp.py が書き出したもの)
- KP : learn/kp.bin

# 現在の進捗

- PST・持ち駒・KP のすべてを差分計算している
  - 玉以外を動かす・打つ手: 増えた駒・減った駒の分だけ足し引き
  - 玉を動かす手: その玉から見た KP を全部計算し直す
- 確認: CFLAGS=-DCHECK_EVAL make でビルドすると、評価のたびに全計算と比べる

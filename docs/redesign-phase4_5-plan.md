# Phase 4.5 詳細計画: Human68k テキスト 4 色対応

> 親プロポーザル: [`redesign-proposal.md`](redesign-proposal.md) §2.1, §3 Phase 4.5, §4.2.6
> 前提: Phase 4 完了（commit `cde7d31`、`TERSE_COLOR_BASIC4` と要求 API 導入済み）

## 0. このフェーズの位置づけ

Phase 4 で導入した「色 5 段階（BASIC4 追加）」と「ケイパビリティ中心設計」の
**最初の実証ユースケース**。Human68k (X68000) を「色 4 / SGR 経由」として登録し、
ケイパビリティに従って色出力が自然に有効化されることを確認する。

検出ロジックの修正が主で、色出力経路は既存の SGR (`^[[Nm`) をそのまま流用する。

### スコープ（確定）
1. `make_human68k_capabilities()` を追加（`colors = TERSE_COLOR_BASIC4`、SGR basic 有効）
2. `detect_environment_capabilities()` に `#if defined(__human68k__)` 分岐を追加
3. `closest_basic4_color()` の 4 色を Human68k テキストパレット初期値（実機）に合わせて確定
4. 上記を検証する単体テスト（terse リポジトリ内で完結）

### スコープ外
- eh68 等の利用側で「色指定を外している箇所の解除」（terse リポジトリ外の別プロジェクト作業）。
  関連: メモリ `project-eh68-raw-mode`
- IOCS マウス・ファンクションキー等の追加ケイパビリティ拾い上げ（必要が出てから）

---

## 1. Human68k テキスト画面の色仕様（調査結果）

`~/src/x68k/human68k_dis_ai/iocscall.md` §テキストパレット初期値 より。

- テキスト画面は **4 プレーン構成**で、各ドットは色コード 0〜15。
  パレットの**下位 16 色** (`$E82200`〜`$E8221F`) を使う
- 色フォーマットは `GGGGG RRRRR BBBBB I`（緑5/赤5/青5 + 共通最下位輝度1bit）
- IPLROM リセット初期化が設定するテキストパレット初期値:

| pal | 値 | G | R | B | 概略色 | RGB8（5bit→8bit, v<<3\|v>>2） |
|-----|-----|---|---|---|--------|------|
| 0 | `$0000` | 0 | 0 | 0 | 黒（背景） | (0, 0, 0) |
| 1 | `$f83e` | 31 | 1 | 31 | シアン寄り | (8, 255, 255) |
| 2 | `$ffc0` | 31 | 31 | 0 | 黄 | (255, 255, 0) |
| 3 | `$fffe` | 31 | 31 | 31 | 白 | (255, 255, 255) |
| 4〜7 | `$de6c` | 27 | 19 | 13 | 明るい灰/淡黄 | (158, 222, 107) |
| 8〜15 | `$4022` | 8 | 1 | 2 | 暗い緑寄り灰 | (8, 66, 16) |

- コンソールは通常 pal[0]=背景, pal[1]=前景（文字色）として使う

### 1.1 4 色をどう選ぶか

テキスト画面で「色」として実用的に区別できるのは初期パレット先頭の **pal[0..3]**:
**黒 / シアン / 黄 / 白**。これを BASIC4 の degrade ターゲット（代表色）とする。

- 根拠: 実機 IPLROM 初期値そのもの。pal[4..15] は灰系の重複なので色としては使わない
- terse のモデル上、BASIC4 端末では任意色を上記 4 色の RGB 最近傍へ寄せる
- SGR 出力経路（basic16 index → `^[[3Nm`）は変更しない。エミュレータ/実機の SGR 実装が
  どの色コードへマップするかは環境依存だが、terse は「最も近い実機表示色」を選ぶ責務だけ負う

---

## 2. 設計判断（AskUserQuestion で確定済み）

| 論点 | 確定方針 |
|------|---------|
| 4 色 capabilities の生成場所 | `detect_environment_capabilities()` 冒頭（P0 チェック直後）で `#if defined(__human68k__)` 分岐し `make_human68k_capabilities()` を返して早期 return。環境変数探索をスキップ |
| 4 色構成 | Human68k テキストパレット初期値 pal[0..3] = 黒/シアン/黄/白（実機 RGB）に確定 |
| スコープ | terse 本体の検出配線 + テストまで。eh68 側の解除は別リポジトリ |

---

## 3. 実装計画

### 3.1 `make_human68k_capabilities()`（`terse_detection.c`）

他の `make_*_capabilities()` と同じ流儀。P0 ベースに色・移動系を載せる:

```c
static terse_capabilities_t
make_human68k_capabilities(void)
{
	terse_capabilities_t caps = terse_make_p0_capabilities();
	caps.profile = TERSE_P1;
	caps.has_size = 1;
	caps.has_sgr_basic = 1;     /* SGR ^[[Nm を受け付ける */
	caps.has_sgr_extended = 0;
	caps.has_truecolor = 0;
	caps.has_text_styles = 1;   /* 反転・太字等は SGR で可能（要確認、最小は color のみ） */
	caps.colors = TERSE_COLOR_BASIC4;  /* ← 検出側が明示。recompute のガードが尊重 */
	/* mouse / title / hyperlink / images / clipboard は無効のまま */
	return caps;
}
```

`colors = TERSE_COLOR_BASIC4` を明示するのが要点。Phase 4 で入れた recompute ガード
（`detected_capabilities.colors == TERSE_COLOR_BASIC4` なら 3 フラグ導出で上書きしない）
がこれを尊重する。`has_sgr_basic=1` だけでは導出が BASIC16 にしてしまうため、この経路が必須。

### 3.2 検出分岐（`detect_environment_capabilities`）

P0 早期 return の直後に挿入:

```c
terse_capabilities_t caps = terse_make_p0_capabilities();
int auto_requested = requested_profile == TERSE_PROFILE_AUTO;
if (!auto_requested && requested_profile == TERSE_P0) {
	return caps;
}
#if defined(__human68k__)
caps = make_human68k_capabilities();
clamp_capabilities_to_request(&caps, requested_profile);
return caps;
#endif
/* 以下、従来の環境変数ベース検出 */
```

`#if` で囲むことで他プラットフォームのコードパスに一切影響しない。
`clamp_capabilities_to_request` でプロファイル要求（P0/P1 等）に合わせて削る。

### 3.3 `closest_basic4_color()` の確定（`terse_style.c`）

Phase 4 の暫定（黒/赤/緑/青、basic16 LUT 流用）を、Human68k 実機パレットに置き換える。
4 色の代表 RGB をローカル定数で持ち、最近傍を返す。返却 kind は basic16 のままだが、
**index ではなく「4 色のどれに寄ったか」を SGR 標準色番号へマップ**して返す:

```c
/* Human68k テキストパレット pal[0..3] の実機初期色（5bit GRB → 8bit RGB）。 */
static const struct { unsigned char r, g, b; terse_basic_color_t sgr; } human68k_text4[4] = {
	{   0,   0,   0, TERSE_BASIC_COLOR_BLACK },   /* pal[0] 黒 */
	{   8, 255, 255, TERSE_BASIC_COLOR_CYAN  },   /* pal[1] シアン */
	{ 255, 255,   0, TERSE_BASIC_COLOR_YELLOW},   /* pal[2] 黄 */
	{ 255, 255, 255, TERSE_BASIC_COLOR_WHITE },   /* pal[3] 白 */
};
```

- 最近傍は二乗距離で選ぶ（既存 `closest_basic16_index` と同じ方式）
- 返す `terse_basic_color_t` は SGR 標準色番号に対応（黒=0/シアン=6/黄=3/白=7）。
  これにより `^[[30m` / `^[[36m` / `^[[33m` / `^[[37m` が出力され、Human68k 上で
  pal[0..3] 相当の表示になることを期待する
- 関数コメントから「暫定」の旨を削除し、根拠（iocscall.md）を明記

### 3.4 degrade の早期 return 例外は維持

Phase 4 で入れた「`support == BASIC4` のときは basic16 入力も素通りさせず 4 色へ寄せる」
ロジック（`terse_style_degrade_color`）はそのまま。4 色構成が変わるだけ。

---

## 4. テスト計画

`tests/unit/` に追加（Phase 4 の `terse_require_test.c` に相補。色縮退は別観点なので
新規ファイル or 既存 style テストに追加するか実装時判断）。

検出側 BASIC4 は `#if defined(__human68k__)` 下なのでホスト（macOS/Linux）の CI では
発火しない。よって**検証は 2 系統**に分ける:

1. **degrade ロジック（ホストで実行可能・内部関数直接）**:
   `terse_style_degrade_color()` に各種色 + `TERSE_COLOR_BASIC4` を渡し、
   黒/シアン/黄/白の SGR 標準色番号へ決定的に寄ることを確認。
   - 例: truecolor(255,255,0) → YELLOW、truecolor(0,255,255) → CYAN、
     truecolor(10,10,10) → BLACK、truecolor(250,250,250) → WHITE
   - 内部ヘッダ `terse_style.h` を include して直接呼ぶ（既存 style テストは公開 API のみ
     だが、4 色マッピングは公開経路で観測する手段が無いため内部直叩きとする。
     実装時に層方針を再確認）
2. **capabilities 構成（条件コンパイル）**:
   `#if defined(__human68k__)` 下でのみ有効なテストとして
   `make_human68k_capabilities()` 相当の検証を書くか、あるいは
   recompute ガードの単体テスト（detected.colors=BASIC4 を直接セットして
   recompute → colors が BASIC4 維持）をホストで書く。後者は Phase 4 計画の
   「BASIC4 recompute ガード」テストに相当し、ホストで実行可能。
   - `terse_capabilities.h` 経由で `recompute_capabilities()` を呼べるか実装時確認

CLAUDE.md の検出テスト作法（`test_env` で環境サニタイズ）に従う。

---

## 5. 作業手順

1. `terse_detection.c`: `make_human68k_capabilities()` 追加 + 前方宣言 + `#if` 検出分岐
2. `terse_style.c`: `closest_basic4_color()` を実機パレット 4 色へ置き換え、コメント更新
3. `tests/unit/`: degrade マッピングテスト + recompute ガードテスト（+ CMakeLists 登録）
4. クリーンビルド（ホスト）+ ctest
5. `include/terse.h` 差分確認（このフェーズはヘッダ変更なしの想定 → 差分ゼロを確認）
6. clang-format フック後に再ビルド確認
7. Conventional Commits でコミット（`feat: ...`）

---

## 6. リスクと留意点

- **SGR 標準色番号 → Human68k 実表示色の対応は環境依存**: terse は「最近傍の実機表示色を選ぶ」
  までが責務。実際の見え方はエミュレータ/実機の SGR 実装に依存する。degrade の選択ロジックを
  1 関数に閉じてあるので、後から実機確認に基づき調整可能
- **クロスコンパイル環境がない**: `#if defined(__human68k__)` 下のコードはホスト CI でビルド
  されない。コンパイルエラーの検出が遅れるリスク。コードは単純に保ち、可能な範囲で
  ホスト実行可能なテスト（recompute ガード）でロジックを担保する
- **has_text_styles の扱い**: Human68k で SGR の太字/反転等が通るか未確認。最小では
  色のみ有効とし、styles は無効から始める判断もあり（実装時に保守的に決める）
- **ヘッダ変更なし**: Phase 4.5 は実装のみ。公開 API は Phase 4 で揃っているため
  `include/terse.h` への変更は発生しない見込み

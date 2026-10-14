//! 用新酷音（libchewing）把測試集的注音轉成中文，模擬使用者逐鍵輸入。
//!
//! 用法：chewing-baseline <詞庫目錄> < data/testset.tsv > results/chewing.tsv
//! 每句都從空白狀態開始，不提交、不學習，取組字區的整句結果。

use std::io::{self, BufRead, Write};

use chewing::editor::{BasicEditor, Editor};
use chewing::input::keymap::{QWERTY_MAP, map_ascii};

/// 標準（大千）鍵盤配置
fn key_of(bopomofo: char) -> u8 {
    match bopomofo {
        'ㄅ' => b'1', 'ㄉ' => b'2', 'ˇ' => b'3', 'ˋ' => b'4', 'ㄓ' => b'5',
        'ˊ' => b'6', '˙' => b'7', 'ㄚ' => b'8', 'ㄞ' => b'9', 'ㄢ' => b'0', 'ㄦ' => b'-',
        'ㄆ' => b'q', 'ㄊ' => b'w', 'ㄍ' => b'e', 'ㄐ' => b'r', 'ㄔ' => b't',
        'ㄗ' => b'y', 'ㄧ' => b'u', 'ㄛ' => b'i', 'ㄟ' => b'o', 'ㄣ' => b'p',
        'ㄇ' => b'a', 'ㄋ' => b's', 'ㄎ' => b'd', 'ㄑ' => b'f', 'ㄕ' => b'g',
        'ㄘ' => b'h', 'ㄨ' => b'j', 'ㄜ' => b'k', 'ㄠ' => b'l', 'ㄤ' => b';',
        'ㄈ' => b'z', 'ㄌ' => b'x', 'ㄏ' => b'c', 'ㄒ' => b'v', 'ㄖ' => b'b',
        'ㄙ' => b'n', 'ㄩ' => b'm', 'ㄝ' => b',', 'ㄡ' => b'.', 'ㄥ' => b'/',
        _ => panic!("未知的注音符號 {bopomofo}"),
    }
}

fn keys_of(syllable: &str) -> Vec<u8> {
    let mut keys: Vec<u8> = syllable.chars().map(key_of).collect();
    if !syllable.ends_with(['ˊ', 'ˇ', 'ˋ', '˙']) {
        keys.push(b' '); // 一聲用空白鍵
    }
    keys
}

fn main() {
    let dict_dir = std::env::args().nth(1).expect("用法：chewing-baseline <詞庫目錄>");
    let mut editor = Editor::chewing(Some(dict_dir), None, &["word.dat", "tsi.dat"]);
    editor.set_editor_options(|opt| opt.disable_auto_learn_phrase = true);

    let mut out = io::stdout().lock();
    for line in io::stdin().lock().lines().skip(1) {
        let line = line.unwrap();
        let cols: Vec<&str> = line.split('\t').collect();
        let (id, zhuyin) = (cols[0], cols[3]);
        editor.clear();
        for syllable in zhuyin.split(' ') {
            for key in keys_of(syllable) {
                editor.process_keyevent(map_ascii(&QWERTY_MAP, key));
            }
        }
        writeln!(out, "{id}\t{}", editor.display()).unwrap();
    }
}

#!/bin/sh
# Self-test of the pre-push hook next to this file: scratch repositories under a temporary folder, a made-up local.json
# (ZAX_LOCAL_JSON; the real one is never read), one commit or tag per case, the hook fed the push line by hand.
# Every case that holds a made-up private value, a home path, an e-mail address or game data must be refused, the clean
# ones must pass. The made-up home paths and addresses are split by '' so this file itself passes the hook. Run: sh tools/git-hooks/test-pre-push.sh   (exit 0 = all cases as expected)
hook=$(cd "$(dirname "$0")" && pwd)/pre-push
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
zero=0000000000000000000000000000000000000000
at=@
noreply="86563507+tester${at}users.noreply.github.com"
cat > "$tmp/local.json" <<EOF
{"share_root": "/Mnt", "share_prefix": "QSHR-", "extra_globs": ["QGLB-*"],
 "lan": {"host": "QVMH-01", "client": "QVMC-01", "client2": "QVMC-02", "host_ip": "10.79.68.57",
         "names": ["Plyrzork", "Plyrwemb"], "game_host_ip": "10.79.68.58"},
 "macvm": {"name": "QMACVM", "host": "10.79.69.5", "user": "qmacuser", "key": "~/.ssh/qmackey"},
 "private_words": ["Quxblorf", "Fnordel"], "exclude": ["QVMX-09"]}
EOF
printf '{"private_words": []}\n' > "$tmp/nowords.json"
export ZAX_LOCAL_JSON="$tmp/local.json"
unset ZAX_PREPUSH_ALL
fails=0 tag= hookenv=

setup() {  # setup <folder name>: a repository with one clean commit, $base
    repo="$tmp/$1"
    git init -q "$repo" && cd "$repo" || exit 1
    git config user.name tester && git config user.email "$noreply" && git config core.hooksPath /dev/null
    echo clean > README.md && git add README.md && git commit -qm "Clean start" && base=$(git rev-parse HEAD)
}
run() {  # run <expected exit> <case> [<remote_sha>]: the hook on HEAD (and on $tag if set)
    want=$1 name=$2 old=${3:-$base}
    line="refs/heads/main $(git rev-parse HEAD) refs/heads/main $old"
    [ -n "$tag" ] && line="$line
refs/tags/$tag $(git rev-parse "refs/tags/$tag") refs/tags/$tag $zero"
    printf '%s\n' "$line" | env $hookenv "$hook" origin url >"$tmp/out" 2>&1
    got=$?
    [ "$got" -ne 0 ] && got=1
    grep -qE 'Traceback|git [a-z-]+ failed' "$tmp/out" && got=error   # a refusal must come from a finding
    if [ "$got" = "$want" ]; then echo "ok    $name"; else echo "FAIL  $name (exit $got, expected $want)"; sed 's/^/      /' "$tmp/out"; fails=$((fails + 1)); fi
    git reset -q --hard "$base"; git tag -d "$tag" >/dev/null 2>&1; tag= hookenv=
}
commit() {  # commit <path> <content printf format> [git commit options...]
    p=$1 c=$2; shift 2
    mkdir -p "$(dirname "$p")" && printf "$c" > "$p" && git add -f "$p" && git commit -q "$@"
}

setup zax-public
commit a.txt 'fine\n' -m 'Clean change' && run 0 "clean commit"
commit a.txt 'fine\n' -m 'Fix for quxBLORF' && run 1 "message: private word (other case)"
commit a.txt 'fine\n' -m 'ok' --author "Fnordel Smith <$noreply>" && run 1 "author name: private word"
commit a.txt 'fine\n' -m 'ok' --author "tester <somebody${at}mail.invalid>" && run 1 "author e-mail: not noreply"
export GIT_COMMITTER_NAME=Quxblorf; commit a.txt 'fine\n' -m 'ok'; unset GIT_COMMITTER_NAME; run 1 "committer name: private word"
commit a.txt 'fine\n' -m 'Seen on QVMH-01' && run 1 "message: VM name"
commit a.txt 'fine\n' -m 'path /Users/''someone/x' && run 1 "message: home path"
commit 'docs/QSHR-01.txt' 'fine\n' -m ok && run 1 "file name: share prefix"
commit 'docs/plyrzork.txt' 'fine\n' -m ok && run 0 "file name: player name in another case (case-sensitive)"
commit 'docs/Plyrzork.txt' 'fine\n' -m ok && run 1 "file name: player name"
commit a.txt 'host 10.79.68.200\n' -m ok && run 1 "content: address /24 prefix"
commit a.txt 'key ~/.ssh/qmackey\n' -m ok && run 1 "content: mac VM key"
commit a.txt 'QVMX-09 is busy\n' -m ok && run 1 "content: excluded VM name"
commit a.txt 'see QGLB-7\n' -m ok && run 1 "content: extra glob"
commit a.txt 'mail somebody''@''mail.invalid\n' -m ok && run 1 "content: e-mail address"
commit a.txt "mail name${at}example.org or $noreply\\n" -m ok && run 0 "content: example.org and noreply addresses"
commit a.txt 'cd /home/''someone/zax\n' -m ok && run 1 "content: /home path"
commit a.txt 'C:\\\\Users\\\\''someone\\\\Temp\n' -m ok && run 1 "content: Windows home path (doubled backslashes)"
commit a.txt 'C:\\Users\\''someone\\Temp\n' -m ok && run 1 "content: Windows home path"
commit a.txt 'C:\\Users\\Zaxvm\\Temp\n' -m ok && run 0 "content: allowed made-up Windows user"
commit a.png '\211PNG\000\000xxQUXBLORFxx\000' -m ok && run 1 "binary: private word"
commit a.png '\211PNG\000\000Q\000u\000x\000b\000l\000o\000r\000f\000' -m ok && run 1 "binary: private word as UTF-16"
commit a.png '\211PNG\000\000/Users/''someone/\000' -m ok && run 1 "binary: home path"
commit a.png '\211PNG\000\000plain\000' -m ok && run 0 "binary: clean"
commit sub/x.RED 'fine\n' -m ok && run 1 "game data: .RED anywhere"
commit sub/model.mdl16 'fine\n' -m ok && run 1 "game data: .mdl16"
commit tools/ZAX.EXE 'fine\n' -m ok && run 1 "game data: Zax.exe"
commit sub/blob.bin 'PK\003\004\024\000\000\000' -m ok && run 1 "game data: zip content"
commit sub/blob.bin 'MZ\220\000\003\000' -m ok && run 1 "game data: exe content"
commit gamedata/readme.txt 'fine\n' -m ok && run 1 "game data: gamedata/"
commit a.txt 'fine\n' -m ok && git tag -a v1 -m 'Release' && tag=v1 && run 0 "clean annotated tag"
commit a.txt 'fine\n' -m ok && git -c user.name='Fnordel' tag -a v1 -m 'Release' && tag=v1 && run 1 "tag: tagger name"
commit a.txt 'fine\n' -m ok && git tag -a v1 -m 'For plyrzork and Quxblorf' && tag=v1 && run 1 "tag: message"
commit a.txt 'fine\n' -m ok && git -c user.email="somebody${at}mail.invalid" tag -a v1 -m 'Release' && tag=v1 && run 1 "tag: tagger e-mail"
# what the remote already has is not checked again (unless ZAX_PREPUSH_ALL=1); a new branch is checked against it
git init -q --bare "$tmp/remote.git" && git remote add origin "$tmp/remote.git"
commit a.txt 'QVMH-01\n' -m 'old' && git push -q --no-verify origin HEAD:refs/heads/main && pushed=$(git rev-parse HEAD)
base=$pushed
commit b.txt 'fine\n' -m 'Clean' && run 0 "new branch on top of what the remote has" "$zero"
commit b.txt 'fine\n' -m 'Clean' && hookenv=ZAX_PREPUSH_ALL=1 run 1 "ZAX_PREPUSH_ALL=1 checks the whole history" "$zero"
commit b.txt 'fine\n' -m 'Clean' && run 0 "unknown old remote value: the remote's refs still exclude what it has" 1111111111111111111111111111111111111111
# the file is required
commit a.txt 'fine\n' -m ok && hookenv="ZAX_LOCAL_JSON=$tmp/missing.json" run 1 "local.json missing"
commit a.txt 'fine\n' -m ok && hookenv="ZAX_LOCAL_JSON=$tmp/nowords.json" run 1 "local.json without private_words"
# zax-redone and zax-testtools may name the VMs, nothing else
setup zax-redone
commit a.txt 'QVMH-01 and QVMC-02\n' -m 'Seen on QVMX-09' && run 0 "private repository: VM names"
commit a.txt 'QSHR-01\n' -m ok && run 1 "private repository: share prefix"
commit a.txt 'fine\n' -m 'Quxblorf' && run 1 "private repository: private word"

[ "$fails" -eq 0 ] && echo "all cases as expected" || echo "$fails case(s) failed"
[ "$fails" -eq 0 ]

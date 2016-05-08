/*
 *  pefnac-utf8 - canfep-like canna frontend processor for UTF-8
 *                Created by Masahiko Ito <m-ito@myh.no-ip.org>
 *
 *  pefnacはcanfep(by Nozomu Kobayashi <nozomu@cup.com>)をオリジナルとしています。
 *
 *  オリジナルからの変更点は以下のとおりです。
 *
 *    o termcapインターフェイスの扱いを改善した(NetBSDでストレス
 *      無く動くことを目標とした)。
 *    o 記述言語をC++からCへ変更した。
 *    o コーディングスタイルを元祖K&Rに変更した。
 *
 *  機能、使用方法等はオリジナルであるcanfepのままです。
 *  canfepのソース、ドキュメントは以下で公開されています。
 *
 *    http://www.geocities.co.jp/SiliconValley-Bay/7584/canfep/
 *
 *  UTF-8対応は以下のパッチを参考にしています。
 *
 *    http://gentoo.osuosl.org/distfiles/canfep_utf8.diff
 *
 *  本ソースは以下の処理により整形されています。
 *
 *    $ indent --version
 *    GNU indent 2.2.6
 *    $ indent -i8 -kr pefnac.c
 *
 */

/*
 * Header files
 */

#if defined(UNIX98)
#	define _XOPEN_SOURCE
#	define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <errno.h>
#if defined(sun) || defined(__NetBSD__)
#include <termcap.h>
#else
#include <term.h>
#endif
#include <errno.h>
#include <iconv.h>
#include <canna/jrkanji.h>

/*
 * Macro definition
 */

/* デフォルトのキー定義 (変えたい場合は man ascii 参照) */
#define FEP_KEY 15
#define ESC_KEY 27

#define	PEFNAC_BUFSIZ	(2048)
#define DEFAULT_TERM	"vt100"
#define DEFAULT_PUTTERM	"TERM=vt100"

/*
 * Typedef definition
 */

typedef void (*SIG_PF) (int);

/*
 * Global variable with initial value
 */

#if 0
char *Amsg = "CANFEP version 1.0 by Nozomu Kobayashi.\nToggleKey=^O\n";
char *Emsg = "CANFEP done!!\n";
#else
char *Amsg = "PEFNAC version 0.4 by Masahiko Ito.\nToggleKey=^O\n";
char *Emsg = "PEFNAC done!!\n";
#endif
char *Nohs[] = { "kon", "jfbterm", (char *) NULL };
char Nullstr[] = "";

/*
 * Global variable without initial value
 */

char Endmsg[PEFNAC_BUFSIZ];
char Buff[PEFNAC_BUFSIZ];
char Funcstr[PEFNAC_BUFSIZ];
char Line[PEFNAC_BUFSIZ];
char *Term;
extern char PC;
extern char *BC;
extern char *UP;
char *So;
char *Se;
char *Us;
char *Ue;
char *Sc;
char *Rc;
char *Ce;
char *Ts;
char *Fs;
char *Ds;
char *Cs;
char *Cl;
char *Cm;
int Hs;
int Fd_put1ch;
int Child;
int Subchild;
int Master;
int Slave;
int Rfd;
int Wfd;
struct termios Tt;
struct termios Stt;
struct winsize Win;
jrKanjiStatusWithValue Ksv;
jrKanjiStatus Ks;
iconv_t Eucjp_to_utf8_cd;
unsigned char SaveMode[PEFNAC_BUFSIZ];
unsigned char CurrentMode[PEFNAC_BUFSIZ];

/*
 * Function declaration
 */

char *Tgetstr();
int put1ch();
void finish();
void winchange();
void Pty();
void get_termcap();
void dPty();
void done();
void fail();
void getmaster();
void getslave();
void fixtty();
void Canna();
void dCanna();
void kakutei();
void henkan();
void delhenkan();
void mode();
void gline();
void write_utf8();
char *iconv_string();
void loop();

/*
 * Main routine
 */
int main(argc, argv)
int argc;
char *argv[];
{
	/* 
	 * 端末初期化 
	 */
	Pty(argc, argv);

	/* 
	 * かんな初期化 
	 */
	Canna();

	/* 
	 * メイン処理 
	 */
	loop();

	/* 
	 * かんな終了処理 
	 */
	dCanna();

	/* 
	 * 端末終了処理 
	 */
	dPty();

	return 0;
}

/*
 * Sub routine
 */

/* コンストラクタだよん */
void Pty(argc, argv)
int argc;
char *argv[];
{
	int ret;
	char *shell;
	int cc;
	char obuf[PEFNAC_BUFSIZ];
	fd_set readfds;
	int t;

	get_termcap();

	/* 開始と終了のメッセージ */
	if (Amsg && argc == 1) {
		write(1, Amsg, strlen(Amsg));
	}
	if (Emsg && argc == 1) {
		strcpy(Endmsg, Emsg);
	} else {
		Endmsg[0] = '\0';
	}

	/* 使用する仮想端末デバイスの名前 */
	/* XX に [p-s][0-f] が入る        */
	strcpy(Line, "/dev/ptyXX");

	/* 仮想端末で起動する shell は何？ */
	shell = getenv("SHELL");
	if (shell == NULL) {
		shell = "/bin/sh";
	}

	/* マスタデバイスの取得 */
	getmaster();

	/* 端末の初期化 */
	fixtty();

	/* フォークします */
	Child = fork();
	if (Child < 0) {
		perror("fork");
		fail();
	}

	/* 子供です */
	if (Child == 0) {

		Subchild = Child = fork();
		if (Child < 0) {
			perror("fork");
			fail();
		}
		if (Child) {
#if defined(__NetBSD__)
/*
 * 起動してもなぜか直に終了してしまう原因はexecしたshellが
 * 起動しきる前に、その疑似端末のマスタ側を読み込もうとし
 * ていたかららしい。NetBSD用と言うより、遅いマシン用と言う
 * べきか。
 */
			sleep(1);
#endif

			close(0);
			FD_ZERO(&readfds);
			FD_SET(Master, &readfds);
			while (1) {
				ret =
				    select(Master + 1, &readfds, 0, 0, 0);
				if (ret == -1) {
					break;
				}
				if (!FD_ISSET(Master, &readfds)) {
					continue;
				}
				cc = (int) read(Master, obuf,
						PEFNAC_BUFSIZ);
				if (cc <= 0) {
					break;
				}
				write(1, obuf, cc);
			}
			done();
			exit(0);
		}
		t = open("/dev/tty", O_RDWR);
		if (t >= 0) {
			ioctl(t, TIOCNOTTY, (char *) 0);
			close(t);
		}
		getslave();
		close(Master);
		dup2(Slave, 0);
		dup2(Slave, 1);
		dup2(Slave, 2);
		close(Slave);
#if defined(__NetBSD__)
/*
 * CTRL-C(SIGINT)等のシグナルが効かなかった原因は、疑似端末の
 * スレーブが制御端末として設定できていなかったかららしい。
 *
 * stdin(=疑似端末のスレーブ)を制御端末とする
 */
		ioctl(0, TIOCSCTTY, 0);
#endif
		if (argc > 1) {
			execvp(argv[1], &argv[1]);
		} else {
			execl(shell, strrchr(shell, '/') + 1, NULL);
		}
		perror(shell);
		fail();
	}

	/* 親です */
	Rfd = 0;
	Wfd = Master;

	/* シグナルハンドラの設定 */
	signal(SIGCHLD, (SIG_PF) finish);
	signal(SIGWINCH, (SIG_PF) winchange);
}

void get_termcap()
{
	int ret;
	char *pt;
	char **nohs;

	/* 環境変数 TERM のエントリを取得 */
	Term = getenv("TERM");
	if (!Term) {
		Term = DEFAULT_TERM;
	}
	ret = tgetent(Buff, Term);
	if (ret != 1) {
		tgetent(Buff, DEFAULT_TERM);
		putenv(DEFAULT_PUTTERM);
	}

	/* termcap から装飾用のエントリを取ってくる */
	pt = Funcstr;

	/* 用意されている幾つかの大局変数の設定 */
	PC = *Tgetstr("pc", &pt);
	BC = Tgetstr("bc", &pt);
	UP = Tgetstr("up", &pt);

	/* スタンドアウト (反転) */
	So = Tgetstr("so", &pt);
	Se = Tgetstr("se", &pt);

	/* アンダーライン (下線) */
	Us = Tgetstr("us", &pt);
	Ue = Tgetstr("ue", &pt);

	/* カーソル位置の保存，保存した位置への復帰 */
	Sc = Tgetstr("sc", &pt);
	Rc = Tgetstr("rc", &pt);

	/* カーソル位置から行の最後までを削除する */
	Ce = Tgetstr("ce", &pt);

	/* カーソルをステータスラインの指定カラムへ移動，元の位置への復帰，ステータスラインの非活性化 */
	Ts = Tgetstr("ts", &pt);
	Fs = Tgetstr("fs", &pt);
	Ds = Tgetstr("ds", &pt);

	/* 画面消去＆カーソルをホームへ移動 */
	Cl = Tgetstr("cl", &pt);

	/* スクロール範囲を指定する */
	Cs = Tgetstr("cs", &pt);

	/* 指定位置にカーソルを移動する */
	Cm = Tgetstr("cm", &pt);

	/* ステータスラインを持っているかどうか */
	Hs = tgetflag("hs");

	/* 幾つかのターミナルではステータスラインを使わない */
	nohs = Nohs;
	while (!nohs) {
		if (strcmp(Term, *nohs) == 0) {
			Hs = 0;
		}
		nohs++;
	}

	/* ステータスラインへ移動，戻る */
	if (!Hs) {
		Fd_put1ch = 1;
		tputs(Ce, 1, put1ch);
		tputs(tgoto(Cs, tgetnum("li") - 2, 0), 1, put1ch);
		tputs(Cl, 1, put1ch);
	}
}

int put1ch(ch)
int ch;
{
	int ret;
	char onech;

	onech = ch;
	ret = write(Fd_put1ch, &onech, 1);
	return ret;
}

/* デストラクタだよん */
void dPty()
{
	done();
	exit(0);
}

/* 終了時に呼ばれる */
void done()
{
	if (Subchild) {
		close(Master);
	} else {
		tcsetattr(0, TCSAFLUSH, &Stt);
	}
}

/* 何が障害があった場合 */
void fail()
{
	kill(0, SIGTERM);
	done();
	exit(1);
}

/* マスタデバイスを取る */
void getmaster()
{
#if defined(UNIX98)
	Master = getpt();
	tcgetattr(0, &Tt);
	Stt = Tt;
	Tt.c_iflag &= ~ISTRIP;
	ioctl(0, TIOCGWINSZ, (char *) &Win);
#else
	struct stat stb;
	char *pty;
	char *p;
	char *s;
	char *t;
	int ok;

	pty = &Line[strlen("/dev/ptyp")];
	for (p = "pqrs"; *p; p++) {
		Line[strlen("/dev/pty")] = *p;
		*pty = '0';
		if (stat(Line, &stb) < 0) {
			break;
		}
		for (s = "0123456789abcdef"; *s; s++) {
			*pty = *s;
			Master = open(Line, O_RDWR);
			if (Master < 0) {
				continue;
			}
			t = &Line[strlen("/dev/")];
			*t = 't';
			ok = access(Line, R_OK | W_OK) == 0;
			*t = 'p';
			if (ok) {
				tcgetattr(0, &Tt);
				Stt = Tt;
				Tt.c_iflag &= ~ISTRIP;
				ioctl(0, TIOCGWINSZ, (char *) &Win);
				return;
			}
			close(Master);
		}
	}

	printf("Out of pty's\n");
	fail();
#endif
}

/* スレーブデバイスを取る */
void getslave()
{
#if defined(UNIX98)
	char *pty;

	pty = ptsname(Master);
	grantpt(Master);
	unlockpt(Master);
	Slave = open(pty, O_RDWR);
	tcsetattr(Slave, TCSAFLUSH, &Tt);
	if (!Hs) {
		Win.ws_row--;
	}
	ioctl(Slave, TIOCSWINSZ, (char *) &Win);
	setsid();
#else
	Line[strlen("/dev/")] = 't';
	Slave = open(Line, O_RDWR);
	if (Slave < 0) {
		perror(Line);
		fail();
	}
	tcsetattr(Slave, TCSAFLUSH, &Tt);
	if (!Hs) {
		Win.ws_row--;
	}
	ioctl(Slave, TIOCSWINSZ, (char *) &Win);
	setsid();
#if !defined(sun)
	close(Slave);
	Slave = open(Line, O_RDWR);
	if (Slave < 0) {
		perror(Line);
		fail();
	}
#endif
#endif				/* UNIX98 */
}

/* 仮想端末の初期化 */
void fixtty()
{
	struct termios rtt;
	rtt = Tt;
	rtt.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF | ISTRIP);
	rtt.c_lflag &= ~(ISIG | ICANON | ECHO);
	rtt.c_oflag &= ~OPOST;
	rtt.c_cc[VMIN] = 1;
	rtt.c_cc[VTIME] = 0;
	tcsetattr(0, TCSAFLUSH, &rtt);
}

/* シグナル受信時に呼ばれる */
void finish(sig)
int sig;
{
	int status;
	int pid;
	int die = 0;

	while ((pid = wait3(&status, WNOHANG, 0)) > 0) {
		if (pid == Child) {
			die = 1;
		}
	}
	if (die) {
		tcsetattr(0, TCSAFLUSH, &Stt);
		Fd_put1ch = 1;
		if (Hs) {
			tputs(Ds, 1, put1ch);
			tputs(Ce, 1, put1ch);
		} else {
			tputs(Cl, 1, put1ch);
			tputs(tgoto(Cs, tgetnum("li") - 1, 0), 1, put1ch);
			tputs(Ce, 1, put1ch);
		}
		if (strlen(Endmsg) != 0) {
			printf("%s", Endmsg);
		}
		exit(0);
	}
}

/* ウィンドウのリサイズ時に呼ばれる */
void winchange(sig)
int sig;
{
	signal(SIGWINCH, SIG_IGN);

	get_termcap();

	/* ウィンドウのサイズを設定し直す (stty -a の 行数/桁数 等) */
	ioctl(0, TIOCGWINSZ, (char *) &Win);
	if (!Hs) {
		Win.ws_row--;
	}
	ioctl(Wfd, TIOCSWINSZ, (char *) &Win);

	signal(SIGWINCH, (SIG_PF) winchange);
}

/*
 * termcapエントリが貧弱な場合core dumpするのを防ぐ為のtgetstr代替
 */
char *Tgetstr(id, area)
char *id;
char **area;
{
	static char *str;
	str = tgetstr(id, area);
	if (str == (char *) NULL) {
		return (Nullstr);
	} else {
		return (str);
	}
}

void write_utf8(int fd, char *p, int len)
{
	if (Eucjp_to_utf8_cd == (iconv_t) - 1)
		write(fd, p, strlen(p));
	else {
		char *putf8 = iconv_string(Eucjp_to_utf8_cd, p, len);
		write(fd, putf8, strlen(putf8));
		free(putf8);
	}
}

char *iconv_string(iconv_t fd, char *str, int slen)
{
	char *from;
	size_t fromlen;
	char *to;
	size_t tolen;
	size_t len = 0;
	size_t done = 0;
	char *result = NULL;
	char *p;

	from = (char *) str;
	fromlen = slen;
	for (;;) {
		if (len == 0 || errno == E2BIG) {
			/* Allocate enough room for most conversions.  When re-allocating
			 * increase the buffer size. */
			len = len + fromlen * 2 + 40;
			p = (char *) malloc((unsigned) len);
			if (p != NULL && done > 0)
				memcpy(p, result, done);
			free(result);
			result = p;
			if (result == NULL)	/* out of memory */
				break;
		}

		to = (char *) result + done;
		tolen = len - done - 2;
		/* Avoid a warning for systems with a wrong iconv() prototype by
		 * casting the second argument to void *. */
		if (iconv(fd, &from, &fromlen, &to, &tolen) !=
		    (size_t) - 1) {
			/* Finished, append a NUL. */
			*to = 0;
			break;
		}
		/* Check both ICONV_EILSEQ and EILSEQ, because the dynamically loaded
		 * iconv library may use one of them. */
		if (errno == EILSEQ || errno == EILSEQ) {
			/* Can't convert: insert a '?' and skip a character.  This assumes
			 * conversion from 'encoding' to something else.  In other
			 * situations we don't know what to skip anyway. */
			*to++ = *from++;
			fromlen -= 1;
		} else if (errno != E2BIG) {
			/* conversion failed */
			free(result);
			result = NULL;
			break;
		}
		/* Not enough room or skipping illegal sequence. */
		done = to - (char *) result;
	}
	return result;
}

/* コンストラクタだよん */
void Canna()
{
	char *p_lang = getenv("LANG");

	/* かんなの初期化 */
	jrKanjiControl(0, KC_INITIALIZE, 0);
	jrKanjiControl(0, KC_SETAPPNAME, "pefnac");
	jrKanjiControl(0, KC_SETBUNSETSUKUGIRI, (char *) &Ksv);
	jrKanjiControl(0, KC_QUERYMODE, (char *) SaveMode);
	jrKanjiControl(0, KC_SETWIDTH, (char *) 72);

	if (p_lang == NULL || strstr(p_lang, "-8"))
		Eucjp_to_utf8_cd = iconv_open("utf-8", "euc-jp");

	mode(SaveMode);
}

/* デストラクタだよん */
void dCanna()
{
	/* かんなの終了処理 */
	jrKanjiControl(0, KC_KILL, (char *) &Ksv);
	jrKanjiControl(0, KC_FINALIZE, 0);

	if (Eucjp_to_utf8_cd != (iconv_t) - 1)
		iconv_close(Eucjp_to_utf8_cd);

	mode(SaveMode);
}

/* 確定した文字列を出力する */
void kakutei(p)
unsigned char *p;
{
	write_utf8(Wfd, (char *) p, strlen((char *) p));
}

/* 変換中(未確定)の文字列を出力する */
void henkan(p, pos, len)
unsigned char *p;
int pos;
int len;
{
	Fd_put1ch = Rfd;
	tputs(Sc, 1, put1ch);
	tputs(Rc, 1, put1ch);
	tputs(Us, 1, put1ch);
	write_utf8(Rfd, (char *) p, pos);
	tputs(Ue, 1, put1ch);
	tputs(So, 1, put1ch);
	write_utf8(Rfd, (char *) p + pos, len);
	tputs(Se, 1, put1ch);
	tputs(Us, 1, put1ch);
	write_utf8(Rfd, (char *) p + pos + len,
		   strlen((char *) p + pos + len));
	tputs(Ue, 1, put1ch);
}

/* 未確定の文字列を削除する */
void delhenkan(len)
int len;
{
	int i;
	Fd_put1ch = Rfd;
	if (len != 0) {
		Fd_put1ch = Rfd;
		tputs(Rc, 1, put1ch);
		tputs(Sc, 1, put1ch);
		for (i = 0; i < len; i++) {
			write(Rfd, " ", 1);
		}
		tputs(Rc, 1, put1ch);
	}
	tputs(Sc, 1, put1ch);
}

/* 現在のモードをステータスラインに出力する */
void mode(p)
unsigned char *p;
{
	Fd_put1ch = Rfd;
	tputs(Sc, 1, put1ch);
	if (Hs) {
		tputs(tgoto(Ts, 0, 0), 1, put1ch);
	} else {
		tputs(tgoto(Cm, 0, tgetnum("li") - 1), 1, put1ch);
	}
	tputs(Ce, 1, put1ch);
	write_utf8(Rfd, (char *) p, strlen((char *) p));
	if (Hs) {
		tputs(Fs, 1, put1ch);
	} else {
		tputs(Rc, 1, put1ch);
	}
}

/* ガイドラインに一覧が表示された時の出力 */
void gline(p, l, pos, len)
unsigned char *p;
unsigned char *l;
int pos;
int len;
{
	Fd_put1ch = Rfd;
	tputs(Sc, 1, put1ch);
	if (Hs) {
		tputs(tgoto(Ts, 0, 0), 1, put1ch);
	} else {
		tputs(tgoto(Cm, 0, tgetnum("li") - 1), 1, put1ch);
	}
	tputs(Ce, 1, put1ch);
	write_utf8(Rfd, (char *) p, strlen((char *) p));
	write(Rfd, " ", 1);
	write_utf8(Rfd, (char *) l, pos);
	tputs(So, 1, put1ch);
	write_utf8(Rfd, (char *) l + pos, len);
	tputs(Se, 1, put1ch);
	write_utf8(Rfd, (char *) l + pos + len,
		   strlen((char *) l + pos + len));
	if (Hs) {
		tputs(Fs, 1, put1ch);
	} else {
		tputs(Rc, 1, put1ch);
	}
}

/* かんなのループ */
void loop()
{
	unsigned char buff[PEFNAC_BUFSIZ];
	int c;
	int nbytes;
	int length;

	buff[0] = '\0';
	/* 最初はアルファベット入力モードにする */
	Ksv.ks = &Ks;
	Ksv.buffer = buff;
	Ksv.bytes_buffer = PEFNAC_BUFSIZ;
	Ksv.val = CANNA_MODE_AlphaMode;
	jrKanjiControl(0, KC_CHANGEMODE, (char *) &Ksv);

	/* 現在のモードを表示 */
	jrKanjiControl(0, KC_QUERYMODE, (char *) CurrentMode);
	mode(CurrentMode);

	nbytes = 0;
	length = 0;
	while (1) {

		/* 一文字受けとる */
		errno = 0;
		c = getchar();
		if (c == EOF) {
			if (errno == 0) {
				continue;
			} else {
				break;
			}
		}

		/* CTRL+SPACE(null) の入力を受け付ける for emacs */
		if (length == 0 && c == '\0') {
			mode(CurrentMode);
			write(Wfd, "\0", 1);
			continue;
		}

		/* 常に C-o を入力モード変更キーにしてしまう (Vine の ~/.canna 対策) */
		if (length == 0 && c == FEP_KEY) {
			if (strcmp((char *) SaveMode, (char *) CurrentMode)
			    == 0) {
				Ksv.val = CANNA_MODE_HenkanMode;
			} else {
				Ksv.val = CANNA_MODE_AlphaMode;
			}
			jrKanjiControl(0, KC_CHANGEMODE, (char *) &Ksv);
			jrKanjiControl(0, KC_QUERYMODE,
				       (char *) CurrentMode);
			mode(CurrentMode);
			continue;
		}

		/* ESC が押されたらアルファベット入力モードにします (vi 用の特殊処理) */
		if (length == 0 && c == ESC_KEY) {
			Ksv.val = CANNA_MODE_AlphaMode;
			jrKanjiControl(0, KC_CHANGEMODE, (char *) &Ksv);
			jrKanjiControl(0, KC_QUERYMODE,
				       (char *) CurrentMode);
			mode(CurrentMode);
			write(Wfd, "\033", 1);
			continue;
		}

		/* 未確定の文字を削除する */
		delhenkan(length);

		/* 一文字渡して変換させる */
		nbytes =
		    jrKanjiString(0, c, (char *) buff, PEFNAC_BUFSIZ, &Ks);

		/* モードに変化があった場合 */
		if (Ks.info & KanjiModeInfo) {
			jrKanjiControl(0, KC_QUERYMODE,
				       (char *) CurrentMode);
			mode(CurrentMode);
		}

		/* ガイドラインに変化があった場合 */
		if (Ks.info & KanjiGLineInfo && Ks.gline.length != 0) {
			gline(CurrentMode, Ks.gline.line,
			      Ks.gline.revPos, Ks.gline.revLen);
		}

		/* 変換が確定した場合 */
		if (nbytes > 0) {
			buff[nbytes] = '\0';
			kakutei(buff);
			buff[0] = '\0';
			length = 0;
		}

		/* 未確定の文字の出力 */
		if (Ks.length > 0) {
			henkan(Ks.echoStr, Ks.revPos, Ks.revLen);
			length = Ks.length;
		}

		/* 未確定の状態で ESC を押したりした時 */
		if (Ks.length < 0) {
			jrKanjiControl(0, KC_KILL, (char *) &Ksv);
			buff[0] = '\0';
			length = 0;
		}
	}
}

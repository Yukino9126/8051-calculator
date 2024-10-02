//讀取keypad，顯示0-f
#include "8051.h"

#define Fosc 12000000
#define Fclk (Fosc / 12)
#define Fint 400
#define Treload (65536 - Fclk / Fint)
#define TH0_R (Treload >> 8)
#define TL0_R (Treload & 0xFF)

__bit func = 0; // 模式(0: 數字模式，1: 功能模式)
signed long int total = 0;
signed long int buffer = 0;
signed long int history[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
signed char historyPtr = 0;

signed char totalFp = -1; // -1: 整數，其餘：小數點位置
signed char bufferFp = -1;
signed char historyFp[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

void handle_func(signed char num);
void flushseg7(signed long int num, signed char fp);

// ---------- Group Assignment ---------- //
const char seg7[16] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c,0x39, 0x5e, 0x79, 0x71 };
char digits[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
signed char num = -1;

void Timer0_ISR(void) __interrupt(1) __using(1) // Timer_0 的 interrupt 編號為 1，當 Timer_0 overflow 會發送 interrupt
{                                               // using register bank 1 (main 用 bank 0)
    static char cnt = 0;
    static char fcnt = 0;
    TH0 = TH0_R;
    TL0 = TL0_R;

    // LED_Display
    // 依序顯示 8 個 digits
    cnt++;
    cnt %= 8;
    // display
    P1 = 7 - cnt;
    P2 = digits[cnt];
    // 功能模式最右邊閃爍
    if(func && cnt == 0){
        if(fcnt < 8)
            P2 = 0x00;
        else
            P2 = digits[0];
        fcnt++;
        fcnt %= 16;
    }
}

void INT0_EXT(void) __interrupt(0) __using(2){ // INT0 external interrupt 編號 0 
    unsigned char i, j;

    P0 = 0xef; // 1110 1111 =>掃描第一行
    for (i = 0; i < 4; i++)
    {
        unsigned char col = 0x01; //用於確認哪個按鈕被按下
        for (j = 0; j < 4; j++)
        {
            if ((P0 & col) == 0)
                num = i * 4 + j;
            col = col << 1;
        }

        //往下一行掃描，0xef(1)、0xdf(2)、0xbf(3)、0x7f(4)
        P0 = (((P0 | 0x0f) << 1) & 0xf0) | 0x0f;
        //P0 = (P0 << 1) + 1;
    }

    P0 = 0xf0;
}

// ------------------------------- //

void main() {

    // ---------- Group Assignment ---------- //
    /* Init Timer_0 (7-Seg Display) */
    TH0 = TH0_R;    // TH0: 高位 8-bit
    TL0 = TL0_R;    // TL0: 低位 8-bit
    // C/T = 0 (timer) M1:M0 = 01 (mode)
    TMOD = 0x01; // 設定 Timer_0 為 Mode 1 (mode 0: 13-bit, mode 1: 16-bit)
    ET0 = 1;     // 啟動 Timer_0 interrupt
    TR0 = 1;     // 啟動 Timer_0

    /* 啟動 INT0 external interrupt (4x4 Keypad) */
    P0 = 0xf0;
    IT0 = 1;
    EX0 = 1;
    
    // enable interrupt
    EA = 1;
    // ------------------------------- //
    
    flushseg7(buffer, bufferFp);
    while (1) {
        // 看 Keypad 有沒有要輸入
        EX0 = 1; // 打開 Keypad
        for(unsigned char i = 0; i < 1; i++); // delay
        EX0 = 0; // 關閉 Keypad

        if(num != -1 && INT0){ // keypad 有輸入
            
            if(func){               /* 輸入功能模式 */
                handle_func(num);
            }
            else if(num < 10){      /* 輸入數字模式 */
                buffer = buffer * 10 + num;
                if(bufferFp != -1)
                    bufferFp++;
                flushseg7(buffer, bufferFp);
            }
            else if(num == 15)      /* 切換功能模式 */
                func = 1;
            else if(num == 10 || num == 11 || num == 13 || num == 14)
                handle_func(num);

            /* 判斷完畢，重設 num */
            num = -1; 
        }
    }

}

void handle_func(signed char num){
    /* 
     *  +---+---+---+---+   U: Up
     *  | + | - | * | / |   D: Down
     *  +---+---+---+---+   E: Enter
     *  | <<| >>| ^ | % |   C: Clear
     *  +---+---+---+---+
     *  | U | E |+/-| <x|
     *  +---+---+---+---+
     *  | D | . | C | = |
     *  +---+---+---+---+
     */

    static signed char last = -1; // 表無上一個運算式
    static __bit histFlag = 1;    // 上次 U/D 後，是否有 Exit
    static signed char historyPtr2 = 0; // 翻閱記錄點

    // ---------- Part1: 處理跟上一步驟無關的功能 ---------- //
    switch (num){
    case 10: /* +/- */
        buffer =  -buffer;
        flushseg7(buffer, bufferFp);
        func = 0;   // 離開功能模式
        return;
    case 13: /* . */
        bufferFp = 0;
        flushseg7(buffer, bufferFp);
        func = 0;   // 離開功能模式
        return;
    case 11: /* <X */
        buffer /= 10;
        if(bufferFp != -1)
            bufferFp--;
        flushseg7(buffer, bufferFp);
        func = 0;   // 離開功能模式
        return;

    case 8:  /* Up */
        if(histFlag)  // 首次進入
            historyPtr2 = historyPtr; // 重設 history 位置
        histFlag = 0;
        // 讀取 history
        buffer = history[historyPtr2];
        bufferFp = historyFp[historyPtr2];
        flushseg7(buffer, bufferFp);
        historyPtr2++;
        historyPtr2 %= 10;
        return;
    case 12: /* Down */
        if(histFlag)  // 首次進入
            historyPtr2 = historyPtr; // 重設 history 位置
        histFlag = 0;
        // 讀取 history
        buffer = history[historyPtr2];
        bufferFp = historyFp[historyPtr2];
        flushseg7(buffer, bufferFp);
        historyPtr2--;
        if(historyPtr2 == -1) historyPtr2 = 9;
        return;
    case 9: /* Exit */
        histFlag = 1;
        func = 0;   // 離開功能模式
        return;

    case 14:  /* C */
        // 歸零
        total = 0;
        totalFp = -1;
        buffer = 0;
        bufferFp = -1;
        last = -1;
        // 輸出結果
        flushseg7(total, totalFp);
        func = 0;   // 離開功能模式
        return;

    default:
        break;
    }

    // ---------- Part2: 處理第一個數值 ---------- //
    if(last == -1){
        total = buffer;
        totalFp = bufferFp;
        buffer = 0;
        bufferFp = -1;
    }

    // ---------- Part3-1: 處理上一步驟輸入的功能(整數) ---------- //
    if(bufferFp == -1 && totalFp == -1){
        switch (last){
        case 0:  /* + */
            total += buffer;
            break;
        case 1:  /* - */
            total -= buffer;
            break;
        case 2:  /* * */
            total *= buffer;
            break;
        case 3:  /* / */
            if(buffer == 0){ // ZeroDivisorError
                flushseg7(0, -1);
                digits[4] = 0x79;
                digits[3] = 0x50;
                digits[2] = 0x50;
                digits[1] = 0x5c;
                digits[0] = 0x50;
                func = 0;   // 離開功能模式
                return;
            }
            total /= buffer;
            break;
        case 4:  /* << */
            total <<= buffer;
            break;
        case 5:  /* >> */
            total >>= buffer;
            break;
        case 6:  /* ^ */
            signed long int tmp = total;
            total = 1;
            while (buffer != 0){
                total *= tmp;
                buffer--;
            }
            break;
        case 7:  /* % */
            total %= buffer;
            break;
        default:
            break;
        }
    }
    // ---------- Part3-2: 處理上一步驟輸入的功能(小數) ---------- //
    else if(last >= 0 || last <= 3){
        // 將兩者都視為小數
        if(totalFp == -1)
            totalFp = 0;
        if(bufferFp == -1)
            bufferFp = 0;
        if(last == 0 || last == 1){ /* +, - */
            // 對齊
            while (totalFp < bufferFp){
                total *= 10;
                totalFp++;
            }
            while (totalFp > bufferFp){
                buffer *= 10;
                bufferFp++;
            }
            // 相加 / 相減
            if(last == 0)
                total += buffer;
            else
                total -= buffer;
        }
        else if(last == 2){ /* * */
            // 相乘
            total *= buffer;
            totalFp += bufferFp;
        }
        else if(last == 3){ /* / */
            if(buffer == 0){ // ZeroDivisorError
                flushseg7(0, -1);
                digits[4] = 0x79;
                digits[3] = 0x50;
                digits[2] = 0x50;
                digits[1] = 0x5c;
                digits[0] = 0x50;
                func = 0;   // 離開功能模式
                return;
            }
            // 準備將整數轉成小數
            float totalDen = 1;
            float bufferDen = 1;
            for(unsigned char i = totalFp; i > 0; i--)
                totalDen *= 10;
            for(unsigned char i = bufferFp; i > 0; i--)
                bufferDen *= 10;
            // 相除
            float tmp = ((float)total / totalDen) / ((float)buffer / bufferDen);
            // 取兩位
            total = (int)((tmp + 0.005) * 100);
            totalFp = 2;
        }

        // 清理 buffer
        buffer = 0;
        bufferFp = -1;
        // 化簡
        while (total % 10 == 0 && totalFp != -1){
            total /= 10;
            totalFp--;
        }
        if(totalFp == 0)
            totalFp = -1;

    }
    // ---------- Part4: 儲存資料 ---------- //
    if(num >= 0 && num <= 7){
        last = num;
        buffer = 0;
        bufferFp = -1;
    }
    else if(num == 15){
        last = -1;
        // 輸出結果
        flushseg7(total, totalFp);
        // 保存 
        historyPtr++;
        historyPtr %= 10;
        history[historyPtr] = total;
        historyFp[historyPtr] = totalFp;
        // 歸零
        total = 0;
        buffer = 0;
        totalFp = -1;
        bufferFp = -1;
    }
    func = 0;   // 離開功能模式

}

void flushseg7(signed long int num, signed char fp){
    // Clean 7-Seg
    for(unsigned char i = 0; i < 8; ++i)
        digits[i] = 0x00;

    unsigned char cnt = 1;
    signed long int numbak = num;
    signed char fpbak = fp;

    /* 數字 */
    if(num >= 0){       /* 正數 */
        digits[0] = seg7[num % 10]; // 個位
        while (num >= 10) {         // 十位、百位、千位...依序 assign 到 digits
            num /= 10;
            digits[cnt] = seg7[num % 10];
            cnt++;
        }
    }
    else{               /* 負數 */
        digits[0] = seg7[-(num % 10)]; // 個位
        while (num <= -10) {           // 十位、百位、千位...依序 assign 到 digits
            num /= 10;
            digits[cnt] = seg7[-(num % 10)];
            cnt++;
        }
        digits[cnt] = 0x40;
    }

    /* 小數點 */
    if(fp != -1){
        while (fp > 0){
            numbak /= 10;
            fp--;
        }
        if(numbak <= 0)
            ;//digits[fpbak] = 0x00; //seg7[0];
        digits[fpbak] |= 0x80;
    }
}
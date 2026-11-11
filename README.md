# Qatwti K10  🐱

A cute desk clock for the **UNIHIKER K10** with a little cat that lives under the clock and reacts to how you move the board.

Written for the Arduino IDE. Landscape screen, 320×240. No microSD card needed.

## What it does

**Clock**
- Big rounded digits with two hearts that beat every second, and a smooth seconds bar.
- The background follows the time of day: pastel daytime with clouds, sunset, night with twinkling stars and a moon, sunrise.
- Sets itself from the internet over WiFi, and shows the date once it knows it.
- A short chime every full hour, and the cat jumps for joy.

**The cat feels movement**

| You do this | The cat does this |
| --- | --- |
| Tilt the board left or right | Slides downhill, leans and looks that way |
| Tilt it steeply | Slides fast with wide eyes — "wheee!" — and bonks into the wall |
| Knock or bump the board | Jumps, surprised, with a "!" |
| Shake it | Hearts, and +1 love point |
| Shake it for a long time | Gets dizzy, stars spin around its head |
| Lay the screen face down | Bed time — it sleeps until you pick it up |
| Leave it alone | Strolls around by itself, then naps after 2 minutes |
| Night (22:00–06:00) | Falls asleep after 15 quiet seconds; any movement wakes it |

**Buttons**

| Button | Action |
| --- | --- |
| A | +1 hour (hold to go fast) |
| B | +1 minute (hold to go fast) |
| A + B together | Drops a fish. The cat runs to eat it (+3 love) |
| Hold A + B for 2 seconds | Temperature correction: then A = −0.5 °C, B = +0.5 °C. It saves itself after 6 seconds |

**Also on screen:** love counter (top left), mood or date (top middle), temperature and humidity (top right). The three RGB lights breathe in pastel colours and change with the cat's mood, and the speaker makes small meows and beeps.

## Getting started

1. Install the Arduino IDE and add the UNIHIKER K10 board by following the [official guide](https://www.unihiker.com/wiki/K10/GettingStarted/gettingstarted_arduinoide/).
2. Download this repository and open `Qatwti_K10/Qatwti_K10.ino`. The file must stay inside a folder named `Qatwti_K10`.
3. Choose the board **unihiker k10** and the right port.
4. Press **Upload**.

If the screen stays black after uploading, see the [K10 FAQ](https://www.unihiker.com/wiki/K10/faq/).

## Setting the time

The clock picks the best source it has, in this order.

**1. WiFi (automatic).** Copy `Qatwti_K10/wifi_secrets.example.h` to `Qatwti_K10/wifi_secrets.h` and write your network name and password in it, then upload. The clock now sets itself every time the board turns on, and checks again twice a day. The network must be 2.4 GHz. `wifi_secrets.h` is in `.gitignore`, so your password is never uploaded with the code. If your timezone is not UTC+3, change `TIMEZONE_HOURS` at the top of the sketch.

**2. From your computer over USB (Windows).** Plug the board in, close the Serial Monitor, and double-click `SET_TIME.bat`. The screen shows "time from PC!".

**3. Neither.** The clock starts from the moment the sketch was compiled, and you can correct it with buttons A and B.

## Settings

All at the top of `Qatwti_K10.ino`.

| Setting | Meaning |
| --- | --- |
| `TIMEZONE_HOURS` | Hours ahead of UTC. Default `3` (Saudi Arabia) |
| `ENABLE_SOUND` | `0` turns the speaker off |
| `ENABLE_SENSORS` | `0` hides temperature and humidity |
| `USE_12H` | `1` for a 12-hour clock |
| `BUMP_SENSITIVITY` | Lower = the cat notices smaller knocks and shakes |
| `TILT_FLIP` | `true` if the cat walks uphill when you tilt |
| `TEMP_OFFSET` | Starting temperature correction in °C |

The board warms its own temperature sensor, so the reading is usually a few degrees high. Use the A + B correction above to match a real thermometer; the correction is remembered after power off.

## Files

```
Qatwti_K10/
  Qatwti_K10.ino            the whole sketch
  wifi_secrets.example.h    template for your WiFi name and password
SET_TIME.bat                double-click to send the PC's time to the board
set_time.ps1                the script behind SET_TIME.bat
```

---

<div dir="rtl">

## بالعربي

**قطوتي K10** ساعة مكتب كيوت للوحة UNIHIKER K10، فيها قطوة صغيرة تعيش تحت الساعة وتتفاعل مع حركة الجهاز.

### القطوة تحس بالحركة

- **ميّلي الجهاز يمين أو يسار:** القطوة تنزلق مع الميلان وتميل براسها وتطالع نفس الاتجاه.
- **ميلان قوي:** تنزلق بسرعة وعيونها مفتوحة وتصدم بالجدار.
- **طقّة أو خبطة على الجهاز:** تنط مفزوعة وتطلع فوقها علامة "!".
- **هزّة:** قلوب و +1 حب.
- **هز كثير:** تدوخ وتدور النجوم حول راسها.
- **الشاشة على وجهها:** وقت النوم، تنام لين ترفعين الجهاز.
- **لو تركتيها:** تتمشى لحالها، وتاخذ غفوة بعد دقيقتين.
- **بالليل (10م – 6ص):** تنام بعد 15 ثانية هدوء، وأي حركة تصحيها.

### الأزرار

- **A:** +ساعة (ضغط مطوّل = سريع).
- **B:** +دقيقة (ضغط مطوّل = سريع).
- **A و B مع بعض:** تنزل سمكة والقطوة تروح تاكلها (+3 حب).
- **A و B ثانيتين:** تصحيح الحرارة، بعدها A ينقص نص درجة و B يزيد نص درجة، ويحفظ لحاله بعد 6 ثواني.

### طريقة التشغيل

1. ثبّتي Arduino IDE وأضيفي لوحة UNIHIKER K10 من [الدليل الرسمي](https://www.unihiker.com/wiki/K10/GettingStarted/gettingstarted_arduinoide/).
2. افتحي الملف `Qatwti_K10/Qatwti_K10.ino` (لازم يبقى داخل مجلد اسمه `Qatwti_K10`).
3. اختاري لوحة **unihiker k10** والمنفذ الصحيح.
4. اضغطي **Upload**.

### ضبط الوقت

- **من الواي فاي (تلقائي):** انسخي الملف `wifi_secrets.example.h` باسم `wifi_secrets.h` واكتبي فيه اسم الشبكة وكلمة السر، ثم ارفعي الكود. الشبكة لازم تكون 2.4GHz. هذا الملف ما ينرفع على GitHub، فكلمة السر تبقى عندك.
- **من الكمبيوتر (USB):** وصّلي الجهاز، سكّري الـ Serial Monitor، واضغطي مرتين على `SET_TIME.bat`.
- **بدون الاثنين:** الساعة تبدأ من وقت تجهيز الكود، وتعدلينها بالأزرار A و B.

</div>

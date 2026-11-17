# -*- coding: utf-8 -*-
Add-Type -AssemblyName System.Drawing
$out = "C:\Users\79992\Desktop\kurs_os_amortizaciya\otchet\fig"
New-Item -ItemType Directory -Force -Path $out | Out-Null

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([System.Drawing.Color]::White)
    return @{ bmp = $bmp; g = $g }
}
function Save-Canvas($c, $name) {
    $path = Join-Path $out $name
    $c.bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $c.g.Dispose()
    $c.bmp.Dispose()
}

$font = New-Object System.Drawing.Font "Times New Roman", 16
$fontB = New-Object System.Drawing.Font "Times New Roman", 16, ([System.Drawing.FontStyle]::Bold)
$fontS = New-Object System.Drawing.Font "Times New Roman", 14
$fontT = New-Object System.Drawing.Font "Times New Roman", 18, ([System.Drawing.FontStyle]::Bold)
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 2
$brush = [System.Drawing.Brushes]::Black
$fillA = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(230, 230, 230))
$fillB = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 255))
$fillC = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(245, 245, 245))
$sf = New-Object System.Drawing.StringFormat
$sf.Alignment = [System.Drawing.StringAlignment]::Center
$sf.LineAlignment = [System.Drawing.StringAlignment]::Center
$sfl = New-Object System.Drawing.StringFormat
$sfl.Alignment = [System.Drawing.StringAlignment]::Near
$sfl.LineAlignment = [System.Drawing.StringAlignment]::Center

function Draw-Box($g, $x, $y, $w, $h, $text, $fill, $f) {
    $rect = New-Object System.Drawing.RectangleF ([single]$x), ([single]$y), ([single]$w), ([single]$h)
    $g.FillRectangle($fill, $rect)
    $g.DrawRectangle($pen, $x, $y, $w, $h)
    $g.DrawString($text, $f, $brush, $rect, $sf)
}
function Draw-Arrow($g, $x1, $y1, $x2, $y2) {
    $cap = New-Object System.Drawing.Drawing2D.AdjustableArrowCap 5.5, 6.5
    $p = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 1.7
    $p.CustomEndCap = $cap
    $g.DrawLine($p, [single]$x1, [single]$y1, [single]$x2, [single]$y2)
    $p.Dispose()
}
function Draw-Line($g, $x1, $y1, $x2, $y2) {
    $g.DrawLine($pen, [single]$x1, [single]$y1, [single]$x2, [single]$y2)
}
function Draw-Stick($g, $x, $y) {
    $g.DrawEllipse($pen, $x - 16, $y, 32, 32)
    $g.DrawLine($pen, $x, ($y + 32), $x, ($y + 78))
    $g.DrawLine($pen, ($x - 26), ($y + 50), ($x + 26), ($y + 50))
    $g.DrawLine($pen, $x, ($y + 78), ($x - 22), ($y + 118))
    $g.DrawLine($pen, $x, ($y + 78), ($x + 22), ($y + 118))
}

# --- use case ---
$c = New-Canvas 1600 1040
$g = $c.g
$g.DrawRectangle($pen, 250, 40, 1100, 960)
$g.DrawString("Модуль учёта амортизации", $fontB, $brush, (New-Object System.Drawing.RectangleF 250, 48, 1100, 36), $sf)
Draw-Stick $g 120 150
$g.DrawString("Бухгалтер", $fontB, $brush, (New-Object System.Drawing.RectangleF 20, 280, 200, 30), $sf)
Draw-Stick $g 1480 470
$g.DrawString("Смотритель", $fontB, $brush, (New-Object System.Drawing.RectangleF 1360, 600, 230, 30), $sf)

$left = @(
    @(340, 120, "Войти в модуль"),
    @(340, 230, "Вести справочник МОЛ"),
    @(340, 340, "Принять объект к учёту"),
    @(340, 450, "Закрыть месяц"),
    @(340, 560, "Отменить закрытие"),
    @(340, 670, "Открыть карточку")
)
$right = @(
    @(860, 120, "Сводка и ведомость"),
    @(860, 230, "Отчёт за месяц и итоги года"),
    @(860, 340, "Прогноз амортизации"),
    @(860, 450, "Списать объект"),
    @(860, 560, "Журнал и выгрузка CSV"),
    @(860, 670, "Копия и учебные данные")
)
foreach ($b in $left) {
    $g.FillEllipse($fillC, $b[0], $b[1], 400, 78)
    $g.DrawEllipse($pen, $b[0], $b[1], 400, 78)
    $g.DrawString($b[2], $fontS, $brush, (New-Object System.Drawing.RectangleF $b[0], $b[1], 400, 78), $sf)
    Draw-Line $g 150 250 $b[0] ($b[1] + 39)
}
foreach ($b in $right) {
    $g.FillEllipse($fillC, $b[0], $b[1], 400, 78)
    $g.DrawEllipse($pen, $b[0], $b[1], 400, 78)
    $g.DrawString($b[2], $fontS, $brush, (New-Object System.Drawing.RectangleF $b[0], $b[1], 400, 78), $sf)
    Draw-Line $g 1450 540 1260 ($b[1] + 39)
}
$g.DrawString("запись", $fontS, $brush, 170, 170)
$g.DrawString("чтение", $fontS, $brush, 1265, 500)
$g.DrawString("Учебная фирма: ООО «Кровля-Сервис»", $fontS, $brush, (New-Object System.Drawing.RectangleF 250, 930, 1100, 40), $sf)
Save-Canvas $c "usecase.png"

# --- classes ---
$c = New-Canvas 1600 1180
$g = $c.g
function ClassBox($g, $x, $y, $w, $title, $lines) {
    $head = 36
    $row = 26
    $h = $head + ($lines.Count * $row) + 10
    $g.FillRectangle($fillA, $x, $y, $w, $head)
    $g.FillRectangle($fillB, $x, ($y + $head), $w, ($h - $head))
    $g.DrawRectangle($pen, $x, $y, $w, $h)
    $g.DrawLine($pen, $x, ($y + $head), ($x + $w), ($y + $head))
    $g.DrawString($title, $fontB, $brush, (New-Object System.Drawing.RectangleF $x, $y, $w, $head), $sf)
    $yy = $y + $head + 4
    foreach ($ln in $lines) {
        $g.DrawString($ln, $fontS, $brush, ($x + 10), $yy)
        $yy += $row
    }
    return $h
}
$h1 = ClassBox $g 40 40 360 "Asset" @("inv, title, place", "personId", "kind, status, method", "accepted, costKop, liqKop", "lifeMonths, declineX100", "planUnits")
$h2 = ClassBox $g 460 40 340 "Charge" @("assetId", "period (year, month)", "amountKop, bookKop", "units")
$h3 = ClassBox $g 860 40 340 "Person" @("id", "name", "post")
$h4 = ClassBox $g 1240 40 320 "UserAccount" @("login", "passHash (SHA-256)", "role")
$h5 = ClassBox $g 60 420 500 "<<interface>> DepreciationStrategy" @("+ monthlyAmount(DepreciationInput)", "LinearStrategy — равная доля", "DecliningStrategy — процент остатка", "OutputStrategy — доля выработки", "SumOfYearsStrategy — вес года")
$h6 = ClassBox $g 640 380 520 "Service" @("+ acceptAsset(), closeMonth()", "+ retire(), reopenLastMonth()", "+ summary(), periodReport(), yearTotals()", "+ forecast(), sheet(), cardText()", "+ seedDemo(), journal()", "правила фирмы и лимит")
$h7 = ClassBox $g 60 780 440 "<<interface>> Store" @("+ migrate(), backupTo()", "+ assets, charges, people", "+ audit, settings, users, journal")
$h8 = ClassBox $g 560 790 380 "Report (CSV)" @("+ exportSheetCsv()", "+ exportPeriodCsv()", "BOM UTF-8, разделитель ;")
$h9 = ClassBox $g 980 860 300 "SqliteStore" @("файл .db", "параметризованный SQL")
$h10 = ClassBox $g 1320 860 240 "SqlServerStore" @("ODBC", "Unicode", "те же операции Store")
Draw-Arrow $g 640 470 560 470
Draw-Arrow $g 900 600 300 780
Draw-Arrow $g 700 600 700 790
Draw-Arrow $g 500 930 980 920
Draw-Arrow $g 1280 910 1320 910
$g.DrawString("использует", $fontS, $brush, 565, 435)
$g.DrawString("хранит", $fontS, $brush, 600, 700)
$g.DrawString("читает", $fontS, $brush, 715, 725)
$g.DrawString("реализует", $fontS, $brush, 760, 945)
$g.DrawString("реализует", $fontS, $brush, 1285, 830)
Save-Canvas $c "classes.png"

# --- state ---
$c = New-Canvas 1500 620
$g = $c.g
$g.FillEllipse($brush, 70, 250, 28, 28)
Draw-Arrow $g 98 264 220 264
Draw-Box $g 220 210 280 110 "Проект карточки`nпроверка даты, суммы,`nинвентарного номера" $fillC $font
Draw-Arrow $g 500 264 640 180
Draw-Arrow $g 500 264 640 420
Draw-Box $g 640 110 340 140 "В учёте, основное средство`nначисление со следующего`nмесяца после принятия" $fillA $font
Draw-Box $g 640 360 340 130 "Малоценка`nстоимость ниже лимита,`nамортизация не начисляется" $fillC $font
Draw-Arrow $g 980 180 1160 264
Draw-Arrow $g 980 420 1160 300
Draw-Box $g 1160 210 280 110 "Списан`nдата и причина,`nначисления закрыты" $fillA $font
$g.DrawString("стоимость от лимита", $fontS, $brush, 500, 140)
$g.DrawString("ниже лимита 100 000 руб.", $fontS, $brush, 470, 470)
$g.DrawString("выбытие", $fontS, $brush, 1040, 150)
$g.DrawString("выбытие", $fontS, $brush, 1040, 390)
Save-Canvas $c "state.png"

# --- ER ---
$c = New-Canvas 1600 980
$g = $c.g
function Er($g, $x, $y, $w, $title, $lines) {
    $head = 34
    $row = 28
    $h = $head + $lines.Count * $row
    $g.FillRectangle($fillA, $x, $y, $w, $head)
    $g.FillRectangle($fillB, $x, ($y + $head), $w, ($h - $head))
    $g.DrawRectangle($pen, $x, $y, $w, $h)
    $g.DrawLine($pen, $x, ($y + $head), ($x + $w), ($y + $head))
    $g.DrawString($title, $fontB, $brush, (New-Object System.Drawing.RectangleF $x, $y, $w, $head), $sf)
    $yy = $y + $head
    foreach ($ln in $lines) {
        $g.DrawString($ln, $fontS, $brush, ($x + 8), ($yy + 3))
        $yy += $row
    }
}
Er $g 40 40 460 "persons" @("PK id", "full_name", "job_title")
Er $g 560 40 480 "assets" @("PK id", "UK inv_no", "FK person_id", "kind, status, method", "accepted_on, cost_kop, liq_kop", "life_months, decline_x100, plan_units", "place, note, retired_on, retire_reason")
Er $g 1100 40 450 "charges" @("PK id", "FK asset_id", "y, m  UNIQUE(asset, y, m)", "amount_kop, book_kop, units")
Er $g 40 420 460 "users" @("PK id", "UK login", "pass_hash", "role_name")
Er $g 560 460 480 "audit_log" @("PK id", "ts", "user_name", "action, details")
Er $g 1100 420 450 "settings" @("PK key", "value", "limit_kop, company, start_rule")
Draw-Arrow $g 500 120 560 140
$g.DrawString("1", $fontB, $brush, 510, 90)
$g.DrawString("N", $fontB, $brush, 530, 150)
Draw-Arrow $g 1040 160 1100 140
$g.DrawString("1", $fontB, $brush, 1045, 110)
$g.DrawString("N", $fontB, $brush, 1065, 170)
$g.DrawString("Связи проведены по внешним ключам. Удаление карточки и строк начисления в роли бухгалтера запрещено.", $fontS, $brush, 40, 900)
Save-Canvas $c "er.png"

# --- components ---
$c = New-Canvas 1500 780
$g = $c.g
Draw-Box $g 500 30 500 70 "Пользователь: бухгалтер или смотритель" $fillC $fontB
Draw-Arrow $g 750 100 750 160
Draw-Box $g 430 160 640 90 "osmod.exe`nменю, ввод, отчёты и CSV" $fillA $font
Draw-Arrow $g 750 250 750 320
Draw-Box $g 300 320 900 110 "библиотека oscore`nmodel, money, depreciation, report, service, sha256, хранилища" $fillA $font
Draw-Arrow $g 520 430 360 540
Draw-Arrow $g 980 430 1140 540
Draw-Box $g 80 540 460 140 "SQLite`nфайл data/krovlaya.db или demo.db`nрезервная копия" $fillC $font
Draw-Box $g 960 540 460 140 "Microsoft SQL Server`nчерез ODBC`nроли os_accountant, os_reader" $fillC $font
$g.DrawString("один интерфейс Store", $fontS, $brush, 600, 470)
Save-Canvas $c "components.png"

# --- flowchart close month ---
$c = New-Canvas 1100 1280
$g = $c.g
function Oval($g, $x, $y, $w, $h, $t) {
    $g.FillEllipse($fillC, $x, $y, $w, $h)
    $g.DrawEllipse($pen, $x, $y, $w, $h)
    $g.DrawString($t, $font, $brush, (New-Object System.Drawing.RectangleF $x, $y, $w, $h), $sf)
}
function Dia($g, $x, $y, $w, $h, $t) {
    $pts = @(
        (New-Object System.Drawing.Point ($x + $w / 2), $y),
        (New-Object System.Drawing.Point ($x + $w), ($y + $h / 2)),
        (New-Object System.Drawing.Point ($x + $w / 2), ($y + $h)),
        (New-Object System.Drawing.Point $x, ($y + $h / 2))
    )
    $g.FillPolygon($fillC, $pts)
    $g.DrawPolygon($pen, $pts)
    $g.DrawString($t, $fontS, $brush, (New-Object System.Drawing.RectangleF ($x + 30), ($y + 18), ($w - 60), ($h - 36)), $sf)
}
Oval $g 340 20 420 60 "Начало закрытия месяца"
Draw-Arrow $g 550 80 550 120
Dia $g 300 120 500 90 "Объект в учёте и это ОС?"
Draw-Arrow $g 550 210 550 250
Dia $g 300 250 500 90 "Месяц не раньше первого начисления и без дырки?"
Draw-Arrow $g 550 340 550 390
Dia $g 300 390 500 100 "Есть что списывать выше ликвидационной?"
Draw-Arrow $g 550 490 550 540
Draw-Box $g 300 540 500 90 "chargeDepreciation`nстратегия по способу" $fillA $font
Draw-Arrow $g 550 630 550 680
Draw-Box $g 300 680 500 80 "Проверка всего месяца, затем запись charges" $fillB $fontS
Draw-Arrow $g 550 760 550 810
Oval $g 340 810 420 60 "Журнал: сумма месяца"
$g.DrawString("нет: пропуск", $fontS, $brush, 820, 150)
$g.DrawString("нет: отказ", $fontS, $brush, 820, 280)
$g.DrawString("нет: пропуск", $fontS, $brush, 820, 420)
Draw-Arrow $g 800 165 980 165
Draw-Box $g 980 130 90 70 "мимо" $fillC $fontS
Save-Canvas $c "flow_close.png"

# --- flowchart linear ---
$c = New-Canvas 1100 900
$g = $c.g
Oval $g 330 20 440 60 "Линейный способ"
Draw-Arrow $g 550 80 550 130
Draw-Box $g 250 130 600 80 "База = первоначальная − ликвидационная" $fillA $font
Draw-Arrow $g 550 210 550 260
Draw-Box $g 220 260 660 80 "Месячная доля = база / срок, остаток копеек копится" $fillB $font
Draw-Arrow $g 550 340 550 400
Dia $g 250 400 600 120 "Это последний месяц срока?"
Draw-Arrow $g 550 520 400 600
Draw-Arrow $g 850 460 980 600
Draw-Box $g 160 600 420 90 "Сумма = равная доля" $fillC $font
Draw-Box $g 640 600 420 100 "Сумма = доля + хвост копеек,`nне ниже ликвидационной" $fillA $font
Draw-Arrow $g 370 690 500 780
Draw-Arrow $g 850 700 620 780
Oval $g 330 780 440 60 "Балансовая после начисления"
Save-Canvas $c "flow_linear.png"

# --- chart book value ---
$c = New-Canvas 1500 860
$g = $c.g
$g.DrawString("Балансовая стоимость, тыс. руб.", $fontB, $brush, 40, 20)
$leftM = 120
$topM = 80
$width = 1280
$height = 640
$g.DrawLine($pen, $leftM, $topM, $leftM, ($topM + $height))
$g.DrawLine($pen, $leftM, ($topM + $height), ($leftM + $width), ($topM + $height))
# scale 0..1700 thousand
function Yof($v) {
    $topM = 80; $height = 640
    return $topM + $height - ($v / 1700.0) * $height
}
$gaz = @(1640, 1585.3, 1532.5, 1481.4, 1432.0, 1384.3, 1338.1, 1293.5, 1250.4, 1208.7, 1168.5, 1129.5, 1091.9, 1055.5)
$comp = @(186.4, 186.4, 186.4, 186.4, 186.4, 186.4, 186.4, 186.4, 186.4, 186.4, 178.9, 171.4, 163.9)
$labs = @("03.24","04.24","05.24","06.24","07.24","08.24","09.24","10.24","11.24","12.24","01.25","02.25","03.25","04.25")
$n = 14
$penG = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), 2.4
$penC = New-Object System.Drawing.Pen ([System.Drawing.Color]::DimGray), 2.4
$penC.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
$penThin = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(180,180,180)), 1
for ($i = 0; $i -lt 5; $i++) {
    $val = $i * 400
    $yy = [single](Yof $val)
    $g.DrawLine($penThin, $leftM, $yy, ($leftM + $width), $yy)
    $g.DrawString([string]$val, $fontS, $brush, 50, ($yy - 12))
}
for ($i = 0; $i -lt ($n - 1); $i++) {
    $x1 = $leftM + ($i * $width / ($n - 1))
    $x2 = $leftM + (($i + 1) * $width / ($n - 1))
    $g.DrawLine($penG, [single]$x1, [single](Yof $gaz[$i]), [single]$x2, [single](Yof $gaz[$i+1]))
    if ($i -lt ($comp.Count - 1)) {
        $g.DrawLine($penC, [single]$x1, [single](Yof $comp[$i]), [single]$x2, [single](Yof $comp[$i+1]))
    }
}
for ($i = 0; $i -lt $n; $i++) {
    $x = $leftM + ($i * $width / ($n - 1))
    $g.FillEllipse($brush, ($x - 4), ((Yof $gaz[$i]) - 4), 8, 8)
    $g.DrawString($labs[$i], $fontS, $brush, ($x - 28), ($topM + $height + 12))
}
$g.FillRectangle($brush, 980, 40, 28, 8)
$g.DrawString("Газель, уменьшаемый остаток", $fontS, $brush, 1016, 30)
$g.DrawLine($penC, 980, 70, 1008, 70)
$g.DrawString("Компрессор, линейный", $fontS, $brush, 1016, 58)
Save-Canvas $c "chart_book.png"

# console picture
$c = New-Canvas 1500 980
$g = $c.g
$g.Clear([System.Drawing.Color]::FromArgb(12, 16, 24))
$mono = New-Object System.Drawing.Font "Consolas", 15
$green = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(210, 230, 210))
$lines = @(
    "Движок: SQLite",
    "Фирма: ООО «Кровля-Сервис»",
    "Лимит признания ОС, руб.: 100 000,00",
    "",
    "=== закрытие 02.2025 ===",
    "ОС-0001   38 948,43   балансовая 1 129 504,70   уменьшаемый остаток",
    "ОС-0002    7 500,00   балансовая   178 900,00   линейный, равная доля",
    "ОС-0003    6 000,00   балансовая   476 000,00   доля выработки, 150 ч",
    "",
    "=== закрытие 04.2025 ===",
    "ОС-0001   36 395,15   балансовая 1 055 459,40",
    "ОС-0002    7 500,00   балансовая   163 900,00",
    "ОС-0003    8 000,00   балансовая   461 000,00   200 моточасов",
    "",
    "=== повторное закрытие 04.2024 ===",
    "по ОС-0001 месяц 04.2024 уже закрыт",
    "",
    "Списан МЦ-0015",
    "Резервная копия: data/krovlaya-backup.db"
)
$yy = 24
foreach ($ln in $lines) {
    $g.DrawString($ln, $mono, $green, 28, $yy)
    $yy += 32
}
Save-Canvas $c "console.png"

Write-Output "figures done"
Get-ChildItem $out | Select-Object Name, Length

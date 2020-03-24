## Стандарты библиотеки OpenBoApi

Стоит придерживаться одного стиля, особенно автору библиотеки.

### Стиль программирования

* Имена классов, перечислений должны быть написаны в стиле *CamelCase*. Например: *Candle, Bar, Api, News*.
* Имена объектов, методов, функций должны быть написаны в стиле *lower camel case*. Например: *array_candles, news_storage, rsi_period*.
* Имена пространств имен могут быть написаны как в стиле *CamelCase*, так и в стиле *lower camel case*, но стоит придерживаться *lower camel case*.
* Имена глобальных констант стоит писать в стиле *upper camel case*.
* Открывать скобку '{' стоит в тойже строке, где расположен оператор или начинается имя метода/функции. Пример:

```C++

std::string get_symbol_name(uint32_t index) {
//...
}

if(...) {

}

for(...) {

}

while(...) {

}
```

* И т.д. Потом распишем.

### Формирование имен переменных, связанные с индикаторами

* Имя переменной/объекта, которая/который связан с конкретным индикатором (например, с *RSI*), должно начинаться с аббревиатуры индикатора. Пример: *rsi_period, rsi_level, bb_period, bb_factor*.
* Если используется модификация индикатора, стоит расширить его имя. Например, *RSI* с использованием *MMA* в своих рассчетах: *rsi_mma_period*.

### Настройки торгового робота

```
{
	// настройки для брокера intrade.bar
	"intrade_bar":{
		"email":"example@gmail.com",
		"password":"der_parol",
		"demo_account":true,										// использовать демо счет
		"rub_currency":true,
		"number_bars":100,
		"sert_file":"curl-ca-bundle.crt",
		"cookie_file":"intrade-bar.cookie",
		"bets_log_file":"logger/intrade-bar-bets.log",
		"work_log_file":"logger/intrade-bar-https-work.log",
		"websocket_log_file":"logger/intrade-bar-https-work.log",
		"use":true 													// использовать брокера
	},
	// настройки для брокера олимптрейд
	"olymp_trade":{
		"port":8080, 			// порт сервера для подключения к расширению браузера
		"demo_account":true, 	// использовать демо счет
		"use":true 				// использовать брокера
	},
	"news":{
		"sert_file":"curl-ca-bundle.crt"
	},
	"trading_robot":{
		"work_log_file":"logger/trading-robot-work.log"
	}
}
```
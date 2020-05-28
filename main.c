#include <LiquidCrystal.h>

// Utiliza LEDs para sinalizar a condição de funcionamento do ESP8266
const int PINO_LED_ERRO = 13;		// Sinaliza erro na comunicação
const int PINO_LED_OK   = 12;		// Sinaliza que o ESP8266 está operacional

// Quais pino estão ligados ao Display LCD
const int PINO_LCD_RS = 2;
const int PINO_LCD_ENABLE = 3;
const int PINO_LCD_DB4 = 4;
const int PINO_LCD_DB5 = 5;
const int PINO_LCD_DB6 = 6;
const int PINO_LCD_DB7 = 7;

// Parâmetros da porta serial
const unsigned long SERIAL_BAUDRATE = 115200L;
const unsigned long SERIAL_TIMEOUT  = 5000L;
const unsigned long TEMPO_MOSTRANDO_RESPOSTA = 4000L;
bool _mostraComandosNoDisplay;

// Características do LCD
const int LCD_NUMERO_LINHAS = 2;
const int LCD_NUMERO_COLUNAS = 16;

// Instancia a classe e define os pinos conectados ao LCD
// NOTA: R/W sempre LOW - usando 4 bits
LiquidCrystal _lcd(PINO_LCD_RS, PINO_LCD_ENABLE,
                   PINO_LCD_DB4, PINO_LCD_DB5, PINO_LCD_DB6, PINO_LCD_DB7);

// Rede de WiFi do simulador
String _ssidName     = "Simulator Wifi";		// Nome da rede
String _ssidPassword = ""; 						// Sem senha
int _tcpHttpPort     = 80; 						// Porta HTTP na conexão TCP

// Dados para acesso a API
String _siteHost    = "api.covid19api.com";
String _siteURIbase = "/live";
String _paramField1  = "/country/";
String _paramField2  = "/brazil/";
String _paramField3  = "/status/";
String _paramField4  = "/live";


// Inicializa a lógica do LCD e abre o canal de comunicação TCP via WiFi
void setup() {
    // Inicializa a lógica do LED de erro
    pinMode(PINO_LED_ERRO, OUTPUT);
    digitalWrite(PINO_LED_ERRO, LOW);

    // Inicializa o LCD e indica a cidade a ser monitorada
    _lcd.begin(LCD_NUMERO_COLUNAS,LCD_NUMERO_LINHAS);
    _lcd.print("Lab08-Exp2  PLUS");
    _lcd.setCursor(0,1);
    _lcd.print("Sensor --> Nuvem");
    delay(2000);	// Deixa a mensagem inicial visível

    //Inicializa a lógica do ESP8266
    pinMode(PINO_LED_OK, OUTPUT);
    Serial.begin(SERIAL_BAUDRATE);
    Serial.setTimeout(SERIAL_TIMEOUT);
    _mostraComandosNoDisplay = true;		// Mostrar comandos AT no display LCD

    // Confirma que o ESP8266 está operacional, inicializando o módulo
    bool esp8266OK = sendCommandTo8266("AT+RST", "ready");
    if (!esp8266OK) {						// O ESP8266 está operacional?
        digitalWrite(PINO_LED_OK, LOW);		// NÃO, reporta o problema
        _lcd.setCursor(0,1);
        _lcd.print("ERRO no ESP8266!");
        while (1) {	// Bloqueia a execução do programa
            delay(500);
            _lcd.noDisplay();
            delay(500);
            _lcd.display();
        }
    } else {								// SIM, acende o LED verde
        digitalWrite(PINO_LED_OK, HIGH);
        // Conecta ao Simulator de WiFi usando o comando AT+CWJAP
        String loginWiFi = "AT+CWJAP=\"" + _ssidName + "\",\"" + _ssidPassword + "\"";
        sendCommandTo8266(loginWiFi, "OK");

        // Abre o canal de comunicação TCP com o site usando o comando AT+CIPSTART
        String acessoTCP = "AT+CIPSTART=\"TCP\",\"" + _siteHost + "\"," + String(_tcpHttpPort);
        sendCommandTo8266(acessoTCP, "OK");

        delay(2000);
        _mostraComandosNoDisplay = false;	// Não mostra mais os comandos AT no display
    }
}


// Envia os sensores periodicamente para o Thingspeak
void loop() {
    // Enviar sensores para a nuvem
    enviarSensores(temperaturaCelsiusTexto, nivelCaixaDaguaTexto, motorLigadoTexto);
    _mostraComandosNoDisplay = false;

    // Enviar sensores a cada 1 segundo
    delay(1000);
}

/**
 * @brief	Monta o pacote GET e envia os dados dos sensores para a nuvem.
 *			Aguarda resposta ou erro.
 * @param	temperatura		Texto com a temperatura a ser apresentada.
 * @param	nivel			Texto com o nível d'água no reservatório.
 * @param	motor			Texto com o status do motor (ligado ou desligado).
 * @return	TRUE, se o pacote foi enviado para o site com sucesso.
 *			FALSE, se houve alguma falha no envio de dados.
 */
int recebeDados() {
    int controleMotor = -1;	// Assume comando inválido
    purgeESP8266();			// Purga resíduos da comunicação com o ESP8266

    // Constrói a requisição HTTP
    String uriCompleta = _siteHost +
            _siteURIbase +
            _paramField1 +
            _paramField2 +
            _paramField3 +
            _paramField4;

    String httpPacket = "GET " + uriCompleta + " HTTP/1.1\r\nHost: " + _siteHost + "\r\n\r\n";
    int length = httpPacket.length();

    // Envia o tamanho do pacote para o ESP8266
    String tamanhoPacote = "AT+CIPSEND=" + numberToString(length);
    if (!sendCommandTo8266(tamanhoPacote, ">"))			// Recebeu o prompt?
        controleMotor = -2;									// NÃO, acusa erro de comunicação
    else if (!sendCommandTo8266(httpPacket, "SEND OK"))	// Conseguiu enviar a mensagem?
        controleMotor = -3;									// NÃO, acusa erro de comunicação
    else if (!recebeuResposta())							// O servidor começou a enviar a resposta?
        controleMotor = -4;									// NÃO, acusa erro de comunicação
    else if(!Serial.find("\"field4\":"))					// A resposta inclui o campo de controle do motor?
        controleMotor = -5;									// NÃO, acusa erro de comunicação
    else {												// SIM, extrai o comando para o motor
        delay(10);
        char dado = Serial.read();
        if('\"' == dado) {									// Início do comando?
            bool recebeuAlgo = false;							// SIM, extrai a resposta
            int leitura = 0;
            while (Serial.available()) {
                dado = Serial.read();
                if ('"' == dado) {						// Fim do campo 4?
                    if (recebeuAlgo && 					// SIM, valida a resposta
                        ((0 == leitura) || (1 == leitura)))
                        controleMotor = leitura;
                    break;
                } else if ((dado >= '0') && 			// Valor numérico?
                           (dado <= '9')) {
                    recebeuAlgo = true;					// SIM, monta o valor
                    dado -= '0';
                    leitura *= 10;
                    leitura += dado;
                } else {								// NÃO, acusa erro
                    break;
                }
            }
        }
    }
    purgeESP8266();
    return controleMotor;
}


/**
 * @brief	Mostra os dados dos sensores no display LCD:
 *			Linha 1: Temp: xxx°C  ON .
 *			Linha 2: Agua: xxx%   OFF.
 * @param	temperatura		Texto com a temperatura a ser apresentada.
 * @param	nivel			Texto com o nível d'água no reservatório.
 * @param	motor			Se motor ligado ou desligado.
 */
void mostraDisplayLCD(String temperatura, String nivel, bool motor) {
    // _lcd.clear();
    // Escreve na primeira linha a temperatura e motor ligado
    _lcd.setCursor(0,0);
    _lcd.print("Temp: ");
    _lcd.print(temperatura);
    _lcd.print('\xB2');
    _lcd.print("C  ");
    if (motor)			_lcd.print("ON      ");
    else					_lcd.print("        ");
    // Escreve na segunda linha o nível da caixa d'água e motor desligado
    _lcd.setCursor(0,1);
    _lcd.print("Agua: ");
    _lcd.print(nivel);
    _lcd.print("%   ");
    if (!motor)			_lcd.print("OFF     ");
    else					_lcd.print("        ");
}


/**
 * @brief	Envia o comando AT para o ESP8266 e aguarda uma string de resposta.
 *			Se ocorrer algum erro, acende o LED de erro.
 * @param	comando		Comando AT a ser enviado para o ESP8266.
 * @param	aguardar	Resposta a ser aguardada do ESP8266.
 *						Se vazia, não aguarda por uma resposta.
 * @return	TRUE, se recebeu a resposta esperada.
 *			FALSE, o comando falhou e o LED de erro foi aceso.
 */
bool sendCommandTo8266(String comando, char * aguardar) {
    bool sucesso = false;

    // Envia o comando para o ESP8266 e aguarda que seja processado
    Serial.println(comando);
    Serial.flush();
    delay(50);
    // Mostra o comando AT no display LCD, se habilitado para tal
    if (_mostraComandosNoDisplay)	{	// Está mostrando AT no display LCD?
        imprimeDisplay(1, comando);		// SIM, mostra o comando enviado
        imprimeDisplay(2, "");				// Aguarda a resposta
    }

    // Verifica se há uma resposta a ser aguardada
    if (0 == aguardar[0])
        sucesso = true;
    else if (_mostraComandosNoDisplay) {	// Mostrando AT na tela do LCD?
        sucesso = findMostrando(aguardar);
        delay(TEMPO_MOSTRANDO_RESPOSTA);	// SIM, permite a leitura
    } else
        sucesso = Serial.find(aguardar);	// NÃO, apenas aguarda a resposta

    digitalWrite(PINO_LED_ERRO, !sucesso);
    return sucesso;
}


/**
 * @brief	Mostra no display LCD os caracteres recebidos enquanto aguarda
 *          a resposta desejada.  Substitui '\r' por '\xFE' e '\n' por '\xFF'.
 * @param	aguardar	Resposta a ser aguardada do ESP8266.
 *						Se vazia, não aguarda por uma resposta.
 * @return	TRUE, se recebeu a resposta esperada.
 *			FALSE, o comando falhou e o LED de erro foi aceso.
 */
bool findMostrando(char * aguardar) {
    bool sucesso = false;

    if (0 == aguardar[0]) 		// Aguarda alguma resposta?
        sucesso = true;				//
    else {
        unsigned long tempoLimite = millis() + SERIAL_TIMEOUT;
        int index = 0;
        String resposta = "";
        while (millis() <= tempoLimite) {
            if (!Serial.available())		// Recebeu algo?
                delay(1);					// NÃO, aguarda 1ms
            else {						// SIM, processa a resposta
                char recebeu = Serial.read();
                // Deixa os caracteres de controle visíveis no display LCD
                if ('\r' == recebeu)			resposta += '\xFE';
                else if ('\n' == recebeu)		resposta += '\xFF';
                else if (recebeu < ' ')			resposta += '\xFC';
                else 							resposta += recebeu;
                // Verifica se encontrou a palavra procurada
                if (recebeu != aguardar[index]) {	// É uma caracter da palavra?
                    index = 0;						// NÃO, continua aguardando
                } else {							// SIM, verifica se terminou
                    index++;
                    if (0 == aguardar[index]) {			// Encontrou o que procurava?
                        sucesso = true;						// SIM, acusa o sucesso
                        break;
                    }
                }
            }
        }	//  while (millis() <= tempoLimite)
        imprimeDisplay(0, resposta);	// Mostra a resposta
    }	// if (0 == aguardar[0])

    return sucesso;
}


/**
 * @brief	Escreve no display LCD as mensagens trocadas com o ESP8266
 *			enquanto não estiver conectado com o Smartphone.
 * @param	comoEscrever	Indica a condição de escrita:
 *							=1, limpa a tela antes de escrever na primeira linha.
 *							=2, limpa a segunda linha antes de escrever.
 *							=3, limpa a primeira linha antes de escrever.
 *							Qualquer outro valor, continua com o cursor atual.
 *			texto			Texto a ser escrito no display LCD.
 */
void imprimeDisplay(byte comoEscrever, String texto) {
    if (1 == comoEscrever)		 	_lcd.clear();
    else if (2 == comoEscrever)		limpaLinhaLCD(1);
    else if (3 == comoEscrever)		limpaLinhaLCD(0);
    String textoCurto = texto.substring(0, LCD_NUMERO_COLUNAS);
    _lcd.print(textoCurto);
}


/**
* @brief	Limpa uma linha do display LCD, deixando o cursor na
*			primeira coluna da linha, ao final.
* @param	linha	Linha a ser limpa.
*/
void limpaLinhaLCD(byte linha) {
    _lcd.setCursor(0, linha);
    _lcd.print("                ");
    _lcd.setCursor(0, linha);
}


// Função necessária para corrigir bug em String(int)
String numberToString(int valor) {
    char numero[6];
    sprintf(numero, "%i", valor);
    return String(numero);
}

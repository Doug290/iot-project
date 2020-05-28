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
    _lcd.print("Status COVID-19 - Brazil");
    _lcd.setCursor(0,1);
    _lcd.print("Buscando dados...");
    delay(2000);

    // Inicializa a comunicação com o ESP8266
    Serial.begin(115200);
    Serial.setTimeout(5000);
    sendCommandTo8266("AT", "OK");	// Confirma que o ESP8266 está operando

    // Conecta ao Simulator de Wifi usando o comando AT+CWJAP
    String loginWiFi = "AT+CWJAP=\"" + _ssidName + "\",\"" + _ssidPassword + "\"";
    sendCommandTo8266(loginWiFi, "OK");

    // Abre o canal de comunicação TCP com o site usando o comando AT+CIPSTART
    String acessoTCP = "AT+CIPSTART=\"TCP\",\"" + _siteHost + "\"," + String(_tcpHttpPort);
    sendCommandTo8266(acessoTCP, "OK");
}


// Atualiza a temperatura a cada 10 segundos
void loop() {
    requestDados();								// Requer dados sobre a temperatura
    if (recebeuResposta()) {						// Recebeu a resposta?
        String dadosCovid = BuscarDadosCovid();	// SIM, mostra a temperatura no display LCD
        mostraDisplayLCD(dadosCovid);
    }
    delay(10000);
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

    // Verifica se há uma resposta a ser aguardada
    if (0 == aguardar[0])		    		sucesso = true;
    else if (Serial.find(aguardar))       sucesso = true;
    else      							digitalWrite(PINO_LED_ERRO, HIGH);

    return sucesso;
}

/**
 * @brief	Monta o pacote GET e solicita dados da API.
 *			Aguarda resposta ou erro.
 * @return	TRUE, se o pacote foi enviado para o site com sucesso.
 *			FALSE, se houve alguma falha no envio de dados.
 */
int requestDados() {
    int tipoErro = -1;	// Assume comando inválido
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
    sucesso &= sendCommandTo8266(tamanhoPacote, ">");
    sucesso &= sendCommandTo8266(httpPacket, "SEND OK\r\n");

    return sucesso;
}

// Aguarda a existência de alguma informação proveniente do
// ESP8266 (resposta ao comando get)
bool recebeuResposta() {
    while(!Serial.available())
    {
        delay(1);
    }

    // Lê as informações recebidas até encontrar um \r\n\r\n
    // isso significa que o cabeçalho http foi recebido por completo
    return Serial.find("\r\n\r\n");
}

/**
 * @brief	Busca a temperatura na resposta da API openweather.org.
 *			Consome os dados na leitura ao buscar "temp":
 * @return	Temperatura encontrada ou vazio.
 */
String BuscarDadosCovid(){
    // Variável auxiliar para detecção de timeout
    unsigned int i = 0;
    String outputString = "";

    // Processa a mensagem recebida até encontrar a string temp:
    while (!Serial.find("\"temp\":"))
    { // Não faz nada. Isso é usado para ir ate o ponto
        // correto dentro da mensagem
    }

    // tenta processar as informações recebidas
    while (i<60000) {
        // se existirem informações a serem processadas
        if(Serial.available()) {
            // Lê os caracteres um a um
            char c = Serial.read();
            // Se encontrar um caractere significa que chegamos ao fim
            // da informação que queríamos encontrar, e termina o loop
            if((c==','))
            {
                break;
            }

            // copia o caractere lido para a variável a ser
            // utilizada posteriormente
            outputString += c;
        }
        // incrementa a variável de número de tentativas
        i++;
    }
    return outputString;
}


/**
 * @brief	limpa o buffer do ESP8266 para que não afete a comunicação
 */
void purgeESP8266() {
    while (Serial.available()) {
        char dado = Serial.read();
    }
}

/**
 * @brief	Mostra a temperatura na segunda linha do display.
 * @param	temperatura		Texto com a temperatura a ser apresentada.
 */
void mostraDisplayLCD(String statusCovid) {
  // move o cursor para segunda linha do display
  _lcd.setCursor(0,1);
  // escreve as informações no display
  _lcd.print(statusCovid);  
  _lcd.print('\xB2'); 
  _lcd.print("C        ");
}


// Função necessária para corrigir bug em String(int)
String numberToString(int valor) {
    char numero[6];
    sprintf(numero, "%i", valor);
    return String(numero);
}

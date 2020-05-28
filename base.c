
const long ESP8266_BAUDRATE = 115200L;

const unsigned long DEFAULT_TIMEOUT = 5000L;

String _dadosRecebidos;

void setup()
{
    // Inicializa as portas de comunicação serial
    Serial.begin(ESP8266_BAUDRATE);
    while(!Serial) {
        delay(100);
    }
    Serial.setTimeout(5000);
    // Mostra BUG em Serial(int)
    Serial.println("Mostra BUG encontrado");
    int numero130 = 130;
    Serial.print("Imprimindo 130 usando print() = ");
    Serial.println(numero130);

    String numero130string = String(numero130);
    Serial.print("Imprimindo 130 usando String(130) = ");
    Serial.println(numero130string);

    Serial.print("Imprimindo 130 usando numberToString() = ");
    String numero130texto = numberToString(numero130);
    Serial.println(numero130texto);
    delay(2000);

    // Incia o teste propriamente dito
    Serial.println();
    Serial.println();
    Serial.println("LAB07-experimento1: Usando o ESP8266 (acessando um site na web)");
    _dadosRecebidos = "";
}

// Envia um comando AT listado a cada 10 segundos
void loop(){
    Serial.println();
    enviaComandoAT("AT+RST", DEFAULT_TIMEOUT, false, false, true);
    enviaComandoAT("AT", DEFAULT_TIMEOUT, true, false, true);
    enviaComandoAT("AT+GMR", DEFAULT_TIMEOUT, true, false, true);
    //enviaComandoAT("AT+CWMODE=1", DEFAULT_TIMEOUT, true, false, true);
    enviaComandoAT("AT+CWLAP", DEFAULT_TIMEOUT, true, false, true);

    // Conecta ao Simulator de Wifi usando o comando AT+CWJAP
    String ssidName     = "Simulator Wifi";		// Nome da rede
    String ssidPassword = ""; 					// Sem senha
    String loginWiFi    = "AT+CWJAP=\"" + ssidName + "\",\"" + ssidPassword + "\"";
    enviaComandoAT(loginWiFi, DEFAULT_TIMEOUT, true, false, true);

    // Mostra o IP do ESP8266
    enviaComandoAT("AT+CIFSR", DEFAULT_TIMEOUT, true, false, true);
    //enviaComandoAT("AT+CWMODE=3", DEFAULT_TIMEOUT, true, false, true);

    // Abre o canal de comunicação TCP com o site usando o comando AT+CIPSTART
    int tcpHttpPort  = 80; 						// Porta HTTP na conexão TCP
    String siteHost  = "api.openweathermap.org";	// URL
    String acessoTCP = "AT+CIPSTART=\"TCP\",\"" + siteHost + "\"," + numberToString(tcpHttpPort);
    enviaComandoAT(acessoTCP, DEFAULT_TIMEOUT, true, false, true);

    // Solicita dados da site usando GET do HTTP
    String siteCity   = "Sorocaba";								// Cidade a ser consultada
    String siteAPPID  = "82bebfe9a9c2686f2597c42c69c94233"; // "cebc60ca0120bfe26aaadcc54c82481e";		// Appkey de acesso ao site
    String siteURI    = "/data/2.5/weather?q=" +
                        siteCity + ",br&APPID=" +
                        siteAPPID + "&units=metric";
    String httpPacket = "GET " + siteURI + " HTTP/1.1\r\nHost: " + siteHost + "\r\n\r\n";
    int length = httpPacket.length();
    String tamanhoPacote = "AT+CIPSEND=" + numberToString(length);
    enviaComandoAT(tamanhoPacote, DEFAULT_TIMEOUT, false, true, false);
    enviaComandoAT(httpPacket, 60000, false, false, true);

    delay(10000);
    Serial.println("=================== REPETINDO ===================");
}

// Envia o comando recebido do Monitor Serial para o ESP8266
void enviaComandoAT(String comando, unsigned long timeout, bool aguardaOK, bool aguardaMaior, bool imprimir) {
    Serial.println(comando);
    _dadosRecebidos += mostraRespostaComandoAT(timeout, aguardaOK, aguardaMaior);
    if (imprimir) {
        if (0 == _dadosRecebidos.length()) {
            Serial.println("ESP8266: sem resposta");
        } else {
            Serial.println(_dadosRecebidos);
            _dadosRecebidos = "";
            Serial.println("-----------------------------------------------------------");
            Serial.println();
            delay(2000);
        }
    }
}

// Mostra a resposta do ESP8266 no Monitor Serial, com aceleradores
String mostraRespostaComandoAT(unsigned long timeout, bool aguardaOK, bool aguardaMaior) {
    if (!aguardaOK && !aguardaMaior) {
        // Algo do header apaga os dados do ESP8266
        while(!Serial.available())
        {
            delay(1);
        }
        // Lê as informações recebidas até encontrar um \r\n\r\n
        // isso significa que o cabeçalho http foi recebido por completo
        //Serial.find("\r\n\r\n");
    }

    // Mostra cada caracter recebido
    unsigned long fim = millis() + timeout;
    bool pronto = false;
    bool recebeuAlgo = false;
    String recebido = "";
    char penultimo = ' ', ultimo = ' ';
    do {
        while (!pronto && Serial.available()) {
            recebeuAlgo = true;
            penultimo = ultimo;
            ultimo = (char) Serial.read();
            recebido += ultimo;
            if (aguardaOK) {
                pronto = ('O' == penultimo) && ('K' == ultimo);
            } else if (aguardaMaior) {
                pronto = ('>' == ultimo);
            } else if (recebido.length() >= 32) {
                // Imprime parcial para não estouorar a string
                Serial.print(recebido);
                recebido = "";
            } else {
                pronto = ('}' == penultimo) && ('\r' == ultimo);
            }
        }
    } while (!pronto && (fim >= millis()));

    return recebido;
}

// Função necessária para corrigir bug em String(int)
String numberToString(int valor) {
    char numero[6];
    sprintf(numero, "%i", valor);
    return String(numero);
}
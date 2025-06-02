#include "Sim7600.h"

//TODO clean & refactor class

Sim7600::Sim7600() : sim7600(2){ //TODO parametrize SERIAL, in this case we use Serial2 but we should be able to choose.
    pinMode(PIN_PWRK, OUTPUT);

    //delay(500);
    digitalWrite(PIN_PWRK, LOW);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    digitalWrite(PIN_PWRK, HIGH);
    sim7600.begin(BAUD_RATE);
   // vTaskDelay(500 / portTICK_PERIOD_MS);
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Mejor esperar 2 segundos a que el SIM esté listo de verdad
}

bool Sim7600::init2(){

    bool result = false;

    AT_CPIN_RESPONSE at_cpin_response = UNDEFINED;

    String response = "";

    sim7600.println("AT+CRESET");
    response = getResponse();
    sim7600.println("AT");
    response = getResponse();
    sim7600.println("AT");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    response = getResponse();

    sim7600.flush();

    bool SIM_PIN_Successful = false;

    Serial.println("aaaaaaaaaaaa");
     //1.
    sim7600.println("AT+CPIN?");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    response = getResponse();

    /*sim7600.println("AT+CSCLK=0"); //don't go into sleep mode
    vTaskDelay(500 / portTICK_PERIOD_MS);
    response = getResponse();
    Serial.println(response);*/

    while(response.isEmpty()){
        Serial.println("SIM7600 not responding, trying to reboot...");
       // digitalWrite(PIN_PWRK, LOW);
        vTaskDelay(500 / portTICK_PERIOD_MS);
     //  digitalWrite(PIN_PWRK, HIGH);
        Serial.println("Reboot completed...");
        sim7600.println("AT+CPIN?");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        response = getResponse();
        Serial.println(response);
        if(response.isEmpty()){
            Serial.println("Still not responding...");
        } else {
            if(response.indexOf("SIM PIN")){
                
            }
            else if(response == "AT\r\r\nOK\r\n"){
                Serial.println(response);
                at_cpin_response = AT_CPIN_RESPONSE_SIM_PIN;
                SIM_PIN_Successful = true;
            } else if(response.indexOf("RDY") != -1){
                at_cpin_response = RDY;
            } else if(response.indexOf("incorrect password") != -1) {
                Serial.println("INCORRECT PASSWORD!!!!!!!!!!!!!!!!!!!!!!!!");
            } else {
                Serial.println("XYZZZZZ!!!!!!!!!!!!!!!!!!!!!!!!");
            }
        }


        //WATCHDOG HERE

    }
    if(response.indexOf("OK") != -1){
        Serial.println(response);
        at_cpin_response = AT_CPIN_RESPONSE_SIM_PIN;
        SIM_PIN_Successful = true;
    } else if(response.indexOf("RDY") != -1){
        at_cpin_response = RDY;
    }


    String AT_CPIN_COMMAND = "AT+CPIN=\"" + SIM_PIN_EXTERN + "\"";

    if(at_cpin_response == AT_CPIN_RESPONSE_SIM_PIN){
        Serial.print("SENDING PIN TO SIM: ");
        Serial.println(SIM_PIN_EXTERN);
        sim7600.println(AT_CPIN_COMMAND);
        response = getResponse();
        Serial.println(response);

        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        } else if(response.indexOf("incorrect password") != -1) {
            Serial.println("INCORRECT PASSWORD!!!!!!!!!!!!!!!!!!!!!!!!");
        } else {
            Serial.println("XYZZZZZ!!!!!!!!!!!!!!!!!!!!!!!!");
        }
    }

    if(at_cpin_response == RDY || at_cpin_response == OK){
        //sim7600.println("AT+CGDCONT=1,\"IP\",\"orangeworld\"");
        sim7600.println("AT+CREG?");
        response = getResponse();
        
        if(response.isEmpty()){
            Serial.println("AT+CREG? not responding...");
        } else {
            Serial.println(response);
            if(response.indexOf("OK") != -1){
                at_cpin_response  = OK;
            }
        }

    } else {

    }

    if(at_cpin_response == OK){
        Serial.println("Todo bien hasta aquí");
    }
    Serial.println("############");
    if(at_cpin_response == OK){
        response = "";
        while(response.isEmpty() || response.indexOf("ERROR") != -1){

           // sim7600.println(AT_CPIN_COMMAND); 
            //response = getResponse();
            //Serial.println(response);




            sim7600.println("AT+CGDCONT=1,\"IP\",\"orangeworld\"");
            response = getResponse();
            Serial.println(response);
            vTaskDelay(500 / portTICK_PERIOD_MS);

            if(response.indexOf("OK") != -1){
                at_cpin_response = OK;
            }
        }
    }


    if(at_cpin_response == OK){
        Serial.println("Todo bien hasta aquí");

sim7600.println("AT+NETCLOSE");
vTaskDelay(500 / portTICK_PERIOD_MS);
getResponse(); // Aunque sea ERROR, no importa

        sim7600.println("AT+NETOPEN");
        response = getResponse();
        Serial.println(response);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }

sim7600.println("AT+IPADDR");
vTaskDelay(500 / portTICK_PERIOD_MS);
String ipResponse = getResponse();
Serial.println(ipResponse);

if (ipResponse.indexOf("0.0.0.0") != -1 || ipResponse.isEmpty()) {
    Serial.println("❌ No hay IP asignada. No podemos continuar.");
    return false;
}        

    } else {

    }

    if(at_cpin_response == OK){
        sim7600.println("AT+IPADDR");
        response = getResponse();
        Serial.println(response);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }
    }


    if(at_cpin_response == OK){
        sim7600.println("AT+CGATT=1");
        response = getResponse();
        Serial.println(response);
        vTaskDelay(500 / portTICK_PERIOD_MS);

        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }
    }

    //MQTT test
    //SET DATA FOR MQTT
    //sim7600.println(AT_CPIN_COMMAND);
    
    return at_cpin_response == OK;
}

bool Sim7600::init3() {
    AT_CPIN_RESPONSE at_cpin_response = UNDEFINED;
    String response;

    sim7600.println("AT+CRESET");
    getResponse(2000);

    // Esperar a que esté listo
    while (true) {
        sim7600.println("AT+CPIN?");
        response = getResponse(1000);
        Serial.println("CPIN Response: " + response);

        if (response.indexOf("SIM PIN") != -1) {
            at_cpin_response = AT_CPIN_RESPONSE_SIM_PIN;
            break;
        } else if (response.indexOf("READY") != -1) {
            at_cpin_response = OK;
            break;
        }

        Serial.println("Esperando SIM...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    if (at_cpin_response == AT_CPIN_RESPONSE_SIM_PIN) {
        String pinCommand = "AT+CPIN=\"" + SIM_PIN_EXTERN + "\"";
        sim7600.println(pinCommand);
        response = getResponse(2000);
        Serial.println("PIN Response: " + response);

        if (response.indexOf("OK") == -1) {
            Serial.println("❌ PIN incorrecto o fallo al enviar PIN");
            return false;
        }

        // Esperar a que diga READY
        while (true) {
            sim7600.println("AT+CPIN?");
            response = getResponse(1000);
            if (response.indexOf("READY") != -1) break;
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }

    // Configurar PDP context
    sim7600.println("AT+CGDCONT=1,\"IP\",\"orangeworld\"");
    response = getResponse(1000);
    Serial.println("CGDCONT: " + response);
    if (response.indexOf("OK") == -1) return false;

    // Cerrar red por si está abierta
    sim7600.println("AT+NETCLOSE");
    getResponse(1000);

    // Abrir red
    sim7600.println("AT+NETOPEN");
    response = getResponse(5000);
    Serial.println("NETOPEN: " + response);

    if (response.indexOf("RDY") != -1 || response.indexOf("+CPIN: SIM PIN") != -1) {
        Serial.println("⚠️  El módem se reinició durante NETOPEN. Reintentando init...");
        return init();
    }

    if (response.indexOf("OK") == -1 && response.indexOf("0") == -1) {
        Serial.println("❌ AT+NETOPEN falló. Estado no válido.");
        return false;
    }

    // Verificar IP
    sim7600.println("AT+IPADDR");
    String ipResponse = getResponse(1000);
    Serial.println("IPADDR: " + ipResponse);

    if (ipResponse.indexOf("0.0.0.0") != -1 || ipResponse.indexOf("ERROR") != -1 || ipResponse.isEmpty()) {
        Serial.println("❌ No hay IP válida asignada."); //TODO <--- aqui darle a reiniciar
        return init();
        //return false;
    }

    // Adjuntar red
    sim7600.println("AT+CGATT=1");
    response = getResponse(3000);
    Serial.println("CGATT: " + response);

    if (response.indexOf("OK") == -1) {
        Serial.println("❌ Error en CGATT");
        return false;
    }

    Serial.println("✅ SIM7600 inicializado correctamente.");
    initialized = true;
    return true;
}



bool Sim7600::init() {
    Serial.println("🔌 Inicializando SIM7600...");

    AT_CPIN_RESPONSE at_cpin_response = UNDEFINED;
    String response = "";

    // Verificar si responde sin hacer reset
    sim7600.println("AT");
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        Serial.println("⛔ SIM7600 no responde. Intentando CRESET...");
        sim7600.println("AT+CRESET");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        getResponse();
        sim7600.println("AT");
        response = getResponse();
        if (response.indexOf("OK") == -1) {
            Serial.println("❌ No se pudo recuperar SIM7600 tras CRESET.");
            return false;
        }
    }

    // Verificar estado del PIN
    sim7600.println("AT+CPIN?");
    response = getResponse();

    if (response.indexOf("SIM PIN") != -1) {
        String AT_CPIN_COMMAND = "AT+CPIN=\"" + SIM_PIN_EXTERN + "\"";
        Serial.println("🔐 Enviando PIN...");
        sim7600.println(AT_CPIN_COMMAND);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        response = getResponse();
        if (response.indexOf("OK") == -1) {
            Serial.println("❌ PIN incorrecto o error al enviar.");
            return false;
        }
    } else if (response.indexOf("READY") != -1 || response.indexOf("OK") != -1) {
        Serial.println("🔓 SIM ya desbloqueada.");
    } else {
        Serial.println("❌ No se pudo verificar el estado del SIM.");
        return false;
    }

    // Configurar APN
    sim7600.println("AT+CGDCONT=1,\"IP\",\"orangeworld\"");
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        Serial.println("❌ Error al configurar APN.");
        return false;
    }

    // Cerrar red por si estaba abierta
    sim7600.println("AT+NETCLOSE");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    getResponse(); // Ignorar errores

    // Abrir red
    sim7600.println("AT+NETOPEN");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        Serial.println("❌ No se pudo abrir la red.");
        return false;
    }

    // Verificar IP asignada
    sim7600.println("AT+IPADDR");
    response = getResponse();
    if (response.indexOf("0.0.0.0") != -1 || response.indexOf("ERROR") != -1) {
        Serial.println("❌ No se obtuvo IP válida.");
        return false;
    }

    Serial.println("✅ SIM7600 inicializado correctamente.");
    initialized = true;
    return true;
}



Result Sim7600::sendHttpPost2(String content, String route){

    Serial.println("SENDING HTTP POST VIA SIM7600");

    //TODO
    /*if(initialized){

    } else {
        Serial.println("Initializing Sim7600 module...");
        while(!init()){
            Serial.println("Could not initialize Sim7600 module...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        initialized = true;
        Serial.println("Sim7600 module initialized...");
    }*/

    //SEND HTTP
    AT_CPIN_RESPONSE at_cpin_response = OK; //TODO
    String response = "";

    if(at_cpin_response == OK){
        String command = "AT+HTTPINIT";
        sim7600.println(command);
        vTaskDelay(1000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
        response = getResponse();
        Serial.println(response);
        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }
    }

    if(at_cpin_response == OK){
        String command = "AT+HTTPPARA=\"URL\",\"http://51.77.195.204:8090/api/iot/login\"";
        sim7600.println(command);
        vTaskDelay(1000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
        response = getResponse();
        Serial.println(response);
        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }
    }

    if(at_cpin_response == OK){
        String command = "AT+HTTPPARA=\"CONTENT\",\"application/json\"";
        sim7600.println(command);
        vTaskDelay(1000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
        response = getResponse();
        Serial.println(response);
        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }
    }

    /*String accountEmail = "email1@gmail.com";
    String accountPassword = "password1";
    String deviceUuid = "esp32uuid_watering_system";
    MessageLogin messageLogin(accountEmail, accountPassword, deviceUuid, "");

    String jsonPayload = messageLogin.toJson();*/
    String jsonPayload = content;

    if(at_cpin_response == OK){

        //String command = "AT+HTTPDATA=50,10000";
        String command = "AT+HTTPDATA=";
        command += jsonPayload.length();
        command += ",";
        command += "10000";
        //String command = "AT+HTTPPARA=\"CONTENT\",\"application/json\"";
        sim7600.println(command);
        vTaskDelay(1000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
        response = getResponse();
        Serial.println(response);
        if(response.indexOf("DOWNLOAD") != -1){
            at_cpin_response = OK;
        }
    }

    if(at_cpin_response == OK){
        sim7600.println("AT+HTTPACTION=1");
        vTaskDelay(10000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
        response = getResponse();
        Serial.println(response);
        if(response.indexOf("OK") != -1){
            at_cpin_response = OK;
        }

        if(at_cpin_response == OK){
            sim7600.println(jsonPayload);
            vTaskDelay(10000 / portTICK_PERIOD_MS); //TODO necessary because it takes some time to process the HTTP request and get the data. For now we just wait 1 second.
            response = getResponse();
            Serial.println(response);
            if(response.indexOf("OK") != -1){
                at_cpin_response = OK;
            }

        }
    }

    return Result(false, "qweqweqw");
}

Result Sim7600::sendHttpPost(String content, String route) {
    Serial.println("📡 SENDING HTTP POST VIA SIM7600");

    if (!initialized) {
        Serial.println("🔄 SIM7600 no inicializado. Intentando iniciar...");
        if (!init()) {
            return Result(false, "❌ No se pudo inicializar el SIM7600.");
        }
    }

    String response = "";
    String fullUrl = "http://51.77.195.204:8090" + route;

    // Asegurar red abierta
    sim7600.println("AT+NETOPEN?");
    response = getResponse();
    Serial.println("NETOPEN?: " + response);
    if (response.indexOf("+NETOPEN: 0") != -1 || response.indexOf("ERROR") != -1) {
        Serial.println("❌ Red cerrada inesperadamente. Intentando reabrir...");
        sim7600.println("AT+NETCLOSE");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        getResponse(); // Ignorar

        sim7600.println("AT+NETOPEN");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        response = getResponse();
        Serial.println("NETOPEN retry: " + response);

        if (response.indexOf("OK") == -1) {
            Serial.println("⛔ Falló el reintento de red. Reiniciando módulo...");
            initialized = false;
            return sendHttpPost(content, route); // Reintenta con init
        }
    }

    // Iniciar HTTP
    sim7600.println("AT+HTTPINIT");
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ HTTPINIT failed");
    }

    // Configurar URL
    sim7600.println("AT+HTTPPARA=\"URL\",\"" + fullUrl + "\"");
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ Error al configurar URL");
    }

    // Configurar Content-Type
    sim7600.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
    response = getResponse();
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ Error al configurar CONTENT");
    }

    // HTTPDATA
    sim7600.println("AT+HTTPDATA=" + String(content.length()) + ",10000");
    response = getResponse();
    if (response.indexOf("DOWNLOAD") == -1) {
        return Result(false, "❌ No se pudo iniciar HTTPDATA");
    }

    sim7600.print(content);
    vTaskDelay(1500 / portTICK_PERIOD_MS);

    // Ejecutar POST
    sim7600.println("AT+HTTPACTION=1");
    vTaskDelay(8000 / portTICK_PERIOD_MS);
    response = getResponse();
    Serial.println("📥 HTTPACTION Response: " + response);

    // Extraer longitud de datos de +HTTPACTION: 1,200,<length>
    int actionIndex = response.indexOf("+HTTPACTION: 1,200,");
    if (actionIndex == -1) {
        return Result(false, "❌ HTTPACTION falló o no fue 200");
    }

    int lenStart = actionIndex + strlen("+HTTPACTION: 1,200,");
    int lenEnd = response.indexOf("\r", lenStart);
    int length = response.substring(lenStart, lenEnd).toInt();

    // Intentar leer los datos con la longitud específica
    for (int i = 0; i < 5; i++) {
        vTaskDelay((1000 + i * 500) / portTICK_PERIOD_MS);
        sim7600.println("AT+HTTPREAD=0," + String(length));
        String httpReadResponse = getResponse(3000);
        Serial.println("📦 HTTPREAD Response attempt " + String(i) + ": " + httpReadResponse);

        if (httpReadResponse.indexOf("ERROR") == -1 && httpReadResponse.indexOf("{") != -1) {
            sim7600.println("AT+HTTPTERM");
            getResponse();
            return parseResult(httpReadResponse);
        }
    }

    sim7600.println("AT+HTTPTERM");
    getResponse();

    return Result(false, "❌ No se pudo leer respuesta HTTP tras múltiples intentos.");
}














/*
String Sim7600::getResponse(){
    String response = "";

   // while(!sim7600.available()){
        vTaskDelay(50 / portTICK_PERIOD_MS);
   // }

    if (sim7600.available()) {
        while(sim7600.available()){
            char character = (char) sim7600.read();
            response += character;
           // if(response.length() > 100) break;
        }
    }

    sim7600.flush();
    return response;
}*/

String Sim7600::getResponse2(){
    esp_task_wdt_reset();
    String response = "";

    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (sim7600.available()) {
        while(sim7600.available()){
            char character = (char) sim7600.read();
            response += character;
        }
    }

    sim7600.flush();

    // Detecta cierre inesperado
    if (response.indexOf("CIPEVENT: NETWORK CLOSED UNEXPECTEDLY") != -1) {
        Serial.println("❌ Red cerrada inesperadamente. Intentando recuperar...");
        String response = "";

        sim7600.println("AT+NETOPEN?");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        getResponse(); // Ignorar respuesta
        response = getResponse();
        Serial.println(response);

        sim7600.println("AT+NETCLOSE");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        response = getResponse(); // Ignorar respuesta
    
        sim7600.println("AT+NETOPEN");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        String openResponse = getResponse();
        Serial.println(openResponse);


    
        if (openResponse.indexOf("OK") != -1) {
            sim7600.println("AT+IPADDR");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            String ipResponse = getResponse();
            Serial.println(ipResponse);
            if (ipResponse.indexOf("0.0.0.0") != -1 || ipResponse.indexOf("ERROR") != -1) {
                Serial.println("❌ IP no válida después de reabrir red. Necesario reiniciar módulo.");
                initialized = false;
            }
        } else {
            Serial.println("❌ No se pudo reabrir red. Necesario reiniciar módulo.");
            initialized = false;
        }
    } else {
        Serial.print("Response: "); Serial.println(response);
    }

    return response;
}


String Sim7600::getResponse(uint32_t timeoutMs) {
    String response;
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        while (sim7600.available()) {
            char c = (char)sim7600.read();
            response += c;
            Serial.print(c);  // 👈 Aquí lo ves en tiempo real
            start = millis();  // reinicia timeout
        }

        if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1)
            break;

        esp_task_wdt_reset();  // evita watchdog
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    sim7600.flush();
    return response;
}


Result Sim7600::mqttPublish(String topic, String message) {
    Serial.println("📡 Inicializando MQTT...");

    String response;

    // Reinicia sesión MQTT por si estaba colgada
    sim7600.println("AT+CMQTTDISC=0,60");
    getResponse(1000);

    sim7600.println("AT+CMQTTREL=0");
    getResponse(1000);

    sim7600.println("AT+CMQTTSTOP");
    getResponse(1000);

    // Inicia sesión MQTT
    sim7600.println("AT+CMQTTSTART");
    response = getResponse(2000);
    Serial.println("CMQTTSTART ➜ " + response);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTSTART falló");
    }

    // Asignar cliente
    sim7600.println("AT+CMQTTACCQ=0,\"SIM7600_Client\"");
    response = getResponse(1000);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTACCQ falló");
    }

    // Conexión al broker
    sim7600.println("AT+CMQTTCONNECT=0,\"tcp://51.77.195.204:1883\",60,1");
    response = getResponse(1000);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTCONNECT falló");
    }

    // Espera a URC +CMQTTCONNECT: 0,0
    bool connected = false;
    for (int i = 0; i < 10; i++) {
        response = getResponse(1000);
        if (response.indexOf("+CMQTTCONNECT: 0,0") != -1) {
            connected = true;
            break;
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    if (!connected) {
        return Result(false, "❌ No llegó URC +CMQTTCONNECT: 0,0");
    }

    // Enviar tópico
    int topicLen = topic.length();
    sim7600.println("AT+CMQTTTOPIC=0," + String(topicLen ));  // +1 para \0
    response = getResponse(1000);
    if (response.indexOf(">") == -1) {
        return Result(false, "❌ No llegó > tras CMQTTTOPIC");
    }
    sim7600.print(topic);
    sim7600.write('\0');
    response = getResponse(1000);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTTOPIC falló tras envío");
    }

    // Enviar payload
    int msgLen = message.length();
    sim7600.println("AT+CMQTTPAYLOAD=0," + String(msgLen));
    response = getResponse(1000);
    if (response.indexOf(">") == -1) {
        return Result(false, "❌ No llegó > tras CMQTTPAYLOAD");
    }
    sim7600.print(message);
    response = getResponse(1000);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTPAYLOAD falló");
    }

    // Publicar
    sim7600.println("AT+CMQTTPUB=0,1,60");
    response = getResponse(2000);
    if (response.indexOf("OK") == -1) {
        return Result(false, "❌ CMQTTPUB falló");
    }

    return Result(true, "✅ MQTT publish exitoso");
}






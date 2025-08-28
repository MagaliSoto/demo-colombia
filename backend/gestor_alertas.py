from twilio.rest import Client

class GestorAlertas:
    def __init__(self, account_sid="", auth_token="", from_wpp="", to_wpp=""):
        
        self.twilio_client = Client(account_sid, auth_token)
        self.from_whatsapp = from_wpp
        self.to_whatsapp = to_wpp

    def enviar_wpp(self, mensaje):
        try:
            self.twilio_client.messages.create(
                body=mensaje,
                from_=self.from_whatsapp,
                to=self.to_whatsapp
            )
            print(f"✅ WhatsApp enviado: {mensaje}")
        except Exception as e:
            print(f"❌ Error enviando WhatsApp: {e}")

    def altertar_evento_entrada(self, id_persona):
        mensaje = f"🚶🏻‍♂️ Persona {id_persona} entro al área."
        self.enviar_wpp(mensaje)        

    def altertar_evento_salida(self, id_persona):
        """
        Se ejecuta solo si la persona estaba previamente en list_persons.
        """
        mensaje = f"🚶🏻‍♂️ Persona {id_persona} salio del área."
        self.enviar_wpp(mensaje)


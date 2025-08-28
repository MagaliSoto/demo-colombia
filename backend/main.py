import argparse, threading, time, json, os, logging, paho.mqtt.client as mqtt
from gestor_alertas import GestorAlertas
from dotenv import load_dotenv

load_dotenv()

# ---------------- CONFIG ----------------
account_sid = os.getenv("TWILIO_ACCOUNT_SID")
auth_token = os.getenv("TWILIO_AUTH_TOKEN")
from_wpp = os.getenv("TWILIO_FROM_WPP")
to_wpp = os.getenv("TWILIO_TO_WPP")

broker_address = "mosquitto-colombia"
broker_port = 1883
topic = "/perception/tracking"

# ---------------- LOGGING ----------------
logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')

# ---------------- GLOBALS ----------------
mqtt_client = mqtt.Client()
person_state = {}  # {person_id: {"last_update": timestamp, "inside_roi": True/False}}

gestor = GestorAlertas(
    account_sid=account_sid,
    auth_token=auth_token,
    from_wpp=from_wpp,
    to_wpp=to_wpp
)

INACTIVITY_THRESHOLD = 4  # segundos sin actualizar antes de considerar salida
CHECK_INTERVAL = 2         # intervalo del thread de verificación

# ---------------- MQTT CALLBACKS ----------------
def on_connect(client, userdata, flags, reason_code, properties=None):
    client.subscribe(topic)
    logging.info(f"Conectado a MQTT y suscrito al topic: {topic}")

def on_message(client, userdata, msg):
    start_time = time.time()
    payload = json.loads(msg.payload.decode('utf-8'))

    person_id = payload["object_id"]
    
    # Actualizar estado o registrar nuevo
    if person_id not in person_state:
        person_state[person_id] = {"last_update": time.time(), "inside_roi": False}
    else:
        person_state[person_id]["last_update"] = time.time()

    # Procesar entrada
    if not person_state[person_id]["inside_roi"]:
        gestor.altertar_evento_entrada(person_id)
        person_state[person_id]["inside_roi"] = True
        logging.info(f"Persona {person_id} entró a la ROI")

    elapsed = time.time() - start_time
    logging.debug(f"on_message() tomó {elapsed:.4f}s para persona {person_id}")

# ---------------- CHECK INACTIVITY ----------------
def check_no_change():
    while True:
        now = time.time()
        for pid, state in list(person_state.items()):
            if state["inside_roi"] and now - state["last_update"] > INACTIVITY_THRESHOLD:
                gestor.altertar_evento_salida(pid)
                state["inside_roi"] = False
                logging.info(f"Persona {pid} salió de la ROI")
        time.sleep(CHECK_INTERVAL)

# ---------------- MAIN ----------------
def main(args):
    threading.Thread(target=check_no_change, daemon=True).start()

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.connect(broker_address, broker_port, 60)
    mqtt_client.loop_forever()

# ---------------- ENTRY POINT ----------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Backend: alertas de entrada/salida en ROI")
    parser.add_argument("--redis_host", type=str, default="redis-colombia", help="Redis host")
    parser.add_argument("--redis_port", type=int, default=6379, help="Redis port")
    parser.add_argument("--redis_db", type=int, default=0, help="Redis database number")
    parser.add_argument("--mosquitto_host", type=str, default="mosquitto-colombia", help="Mosquitto host")
    parser.add_argument("--mosquitto_port", type=int, default=1883, help="Mosquitto port")
    args = parser.parse_args()

    main(args)
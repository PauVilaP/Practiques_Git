# Mosquitto al Raspberry Pi (P-PRE3)

Ordres per instal·lar el broker i deixar-lo com a servei. Port 1883 (Node-RED és al 1880, no hi ha conflicte).

```bash
# 1. Instal·lar el broker i els clients
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

# 2. Crear la configuració pròpia (enganxar-hi el contingut de p3.conf)
sudo nano /etc/mosquitto/conf.d/p3.conf

# 3. Que arrenqui sol amb el Raspberry
sudo systemctl enable mosquitto

# 4. Engegar-lo ara amb la configuració nova
#    (restart perquè apt ja l'engega en instal·lar-lo)
sudo systemctl restart mosquitto

# 5. Comprovar-ho: ha de dir "enabled" i "active (running)"
systemctl status mosquitto
```

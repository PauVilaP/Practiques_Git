# Mosquitto al Raspberry Pi (P3 · exercici 1)

Broker instal·lat amb apt, com a servei de systemd, amb usuaris i ACL.
Port 1883 (Node-RED és al 1880, no hi ha conflicte).

## 1. Instal·lar

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

## 2. Usuaris (cada ordre demana la contrasenya)

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd node01   # -c crea el fitxer: només el primer cop
sudo mosquitto_passwd /etc/mosquitto/passwd nodered
sudo mosquitto_passwd /etc/mosquitto/passwd pau
```

## 3. ACL i configuració (enganxar-hi `acl` i `p3.conf` d'aquesta carpeta)

```bash
sudo nano /etc/mosquitto/acl
sudo nano /etc/mosquitto/conf.d/p3.conf
sudo chown mosquitto: /etc/mosquitto/passwd /etc/mosquitto/acl   # que el broker els pugui llegir
sudo chmod 700 /etc/mosquitto/passwd /etc/mosquitto/acl          # i ningú més
```

## 4. Servei

```bash
sudo systemctl enable mosquitto     # que arrenqui sol amb el Raspberry
sudo systemctl restart mosquitto    # engegar-lo ara amb la configuració nova
systemctl status mosquitto          # ha de dir enabled i active (running)
```

## 5. Comprovar

```bash
# Sense usuari: ha de dir "not authorised"
mosquitto_sub -h localhost -t 'uvic/se2627/pauvila/#' -v

# Amb l'usuari pau: ha d'escoltar
mosquitto_sub -h localhost -u pau -P CONTRASENYA -t 'uvic/se2627/pauvila/#' -v

# Arrenca sol després d'un reinici?
sudo reboot
systemctl status mosquitto
```

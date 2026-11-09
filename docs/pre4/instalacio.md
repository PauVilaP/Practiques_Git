# InfluxDB 1.x i Grafana al Raspberry Pi (P-PRE4)

Ordres per instal·lar InfluxDB i Grafana des dels repositoris oficials i deixar-los com a servei. InfluxDB escolta al port 8086 i Grafana al 3000 (Mosquitto és al 1883 i Node-RED al 1880, no hi ha conflicte).

El Raspberry porta Raspbian de 32 bits (`dpkg --print-architecture` → `armhf`). Per a aquesta arquitectura, el repositori d'InfluxData només té InfluxDB fins a la 1.8.10, que és la que s'instal·la. Grafana: 13.3.0.

```bash
# 1. Paquets necessaris i carpeta de les claus
sudo apt-get update
sudo apt-get install -y apt-transport-https wget gnupg
sudo mkdir -p /etc/apt/keyrings

# 2. Repositori d'InfluxData (es comprova l'empremta de la clau abans de fer-la servir)
wget -q https://repos.influxdata.com/influxdata-archive.key
gpg --show-keys --with-fingerprint --with-colons ./influxdata-archive.key 2>&1 \
  | grep -q '^fpr::*24C975CBA61A024EE1B631787C3D57159FC2F927:$' \
  && cat influxdata-archive.key | gpg --dearmor \
  | sudo tee /etc/apt/keyrings/influxdata-archive.gpg > /dev/null
echo 'deb [signed-by=/etc/apt/keyrings/influxdata-archive.gpg] https://repos.influxdata.com/debian stable main' \
  | sudo tee /etc/apt/sources.list.d/influxdata.list

# 3. Repositori de Grafana
sudo wget -q -O /etc/apt/keyrings/grafana.asc https://apt.grafana.com/gpg-full.key
sudo chmod 644 /etc/apt/keyrings/grafana.asc
echo "deb [signed-by=/etc/apt/keyrings/grafana.asc] https://apt.grafana.com stable main" \
  | sudo tee /etc/apt/sources.list.d/grafana.list

# 4. Instal·lar (el paquet influxdb és la 1.x; no confondre amb influxdb2)
sudo apt-get update
apt-cache policy influxdb grafana
sudo apt-get install -y influxdb grafana

# 5. Que arrenquin sols amb el Raspberry, i engegar-los ara
sudo systemctl unmask influxdb
sudo systemctl daemon-reload
sudo systemctl enable --now influxdb
sudo systemctl enable --now grafana-server

# 6. Usuari administrador d'InfluxDB (abans d'activar l'autenticació)
influx -execute "CREATE USER admin WITH PASSWORD 'posa-la-teva' WITH ALL PRIVILEGES"

# 7. Activar l'autenticació i treure les estadístiques d'ús (veure influxdb.conf)
sudo sed -i 's/^\s*#\s*auth-enabled = false/  auth-enabled = true/' /etc/influxdb/influxdb.conf
sudo sed -i 's/^\s*#\s*reporting-disabled = false/reporting-disabled = true/' /etc/influxdb/influxdb.conf
sudo systemctl restart influxdb

# 8. Grafana: deixar explícits els valors de seguretat (veure grafana.ini)
sudo cp /etc/grafana/grafana.ini /etc/grafana/grafana.ini.orig
sudo sed -i '/^\[server\]/,/^\[/ s/^;\s*http_port\s*=.*/http_port = 3000/' /etc/grafana/grafana.ini
sudo sed -i '/^\[security\]/,/^\[/ s/^;\s*admin_user\s*=.*/admin_user = admin/' /etc/grafana/grafana.ini
sudo sed -i '/^\[users\]/,/^\[/ s/^;\s*allow_sign_up\s*=.*/allow_sign_up = false/' /etc/grafana/grafana.ini
sudo sed -i '/^\[auth.anonymous\]/,/^\[/ s/^;\s*enabled\s*=.*/enabled = false/' /etc/grafana/grafana.ini
sudo sed -i '/^\[analytics\]/,/^\[/ s/^;\s*reporting_enabled\s*=.*/reporting_enabled = false/' /etc/grafana/grafana.ini
sudo systemctl restart grafana-server

# 9. Comprovar-ho: "enabled" i "active", i el ping ha de tornar 204
systemctl is-enabled influxdb grafana-server
systemctl is-active influxdb grafana-server
curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8086/ping
influx -execute "SHOW USERS"                                          # ha de fallar
influx -username admin -password 'posa-la-teva' -execute "SHOW USERS" # admin true

# 10. Prova del reinici: després de sudo reboot, tots dos han de tornar a ser "active"
uptime -s
systemctl is-active influxdb grafana-server
```

Grafana s'obre a `http://<IP del Raspberry>:3000`. La primera vegada s'entra amb `admin` / `admin` i demana una contrasenya nova.

Espai a la targeta abans d'instal·lar (`df -h /`): 14 GB, 4,1 GB usats i 9,1 GB lliures. La instal·lació n'ocupa uns 1,5 GB, gairebé tot de Grafana.

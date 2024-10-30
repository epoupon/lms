- [Installation](#installation)
  * [Docker](#docker)
  * [Debian packages](#debian-packages)
  * [From source](#from-source)
    + [Build dependencies](#build-dependencies)
    + [Build](#build)
    + [Installation](#installation-1)
    + [Upgrade](#upgrade)
- [Deployment](#deployment)
  * [Configuration](#configuration)
  * [Authentication backends](#authentication-backends)
  * [Deploy on non root path](#deploy-on-non-root-path)
  * [Reverse proxy settings](#reverse-proxy-settings)
- [Run](#run)
- [First launch](#first-launch)
# Installation
## Docker
_Docker_ images are available, please see detailed instructions on https://hub.docker.com/r/epoupon/lms.
## Debian packages
_Bookworm_ packages are provided for _amd64_ architectures.
As root, trust the following debian package provider and add it in your list of repositories:
```sh
wget --backups=1 https://debian.poupon.dev/apt/debian/epoupon.gpg -P /usr/share/keyrings
echo "deb [signed-by=/usr/share/keyrings/epoupon.gpg] https://debian.poupon.dev/apt/debian bookworm main" > /etc/apt/sources.list.d/epoupon.list
```
To install or upgrade _LMS_:
```sh
apt update
apt install lms
```
The _lms_ service is started just after the package installation, run by a dedicated _lms_ system user.</br>
Please refer to [Deployment](#deployment) for further configuration options.
## From source
__Note__: this installation process and the default values of the configuration files have been written for _Debian Bookworm_. Therefore, you may have to adapt commands and/or paths in order to fit to your distribution.
### Build dependencies
__Notes__:
* a C++20 compiler is needed
* ffmpeg version 4 minimum is required
```sh
apt-get install g++ cmake libboost-program-options-dev libboost-system-dev libavutil-dev libavformat-dev libstb-dev libconfig++-dev ffmpeg libtag1-dev libpam0g-dev libgtest-dev libarchive-dev
```
__Notes__:
* libpam0g-dev is optional (only for using PAM authentication)
* libstb-dev can be replaced by libgraphicsmagick++1-dev (the latter will likely use more RAM)
You also need _Wt4_, which is not packaged on _Debian_. See [installation instructions](https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html).</br>
No optional requirement is needed, except openSSL if you plan not to deploy behind a reverse proxy (which is not recommended).
### Build
Get the latest stable release and build it:
```sh
git clone https://github.com/epoupon/lms.git lms
cd lms
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
```
__Notes__:
* you can customize the installation directory using `-DCMAKE_INSTALL_PREFIX=path` (defaults to `/usr/local`).
* you can customize the image library using `-DLMS_IMAGE_BACKEND=<stb|graphicsmagick>` (defaults to `stb`)
```sh
make
```
__Note__: you can use `make -jN` to speed up compilation time (N is the number of compilation workers to spawn).
### Installation
__Note__: the commands of this section require root privileges.
```sh
make install
```
Create a dedicated system user:
```sh
useradd --system --group lms
```
Copy the configuration files:
```sh
cp /usr/share/lms/lms.conf /etc/lms.conf
cp /usr/share/lms/lms.service /lib/systemd/system/lms.service
```
Create the working directory and give it access to the _lms_ user:
```sh
mkdir /var/lms
chown lms:lms /var/lms
```
To make _LMS_ run automatically during startup:
```sh
systemctl enable lms
```
### Upgrade
To upgrade _LMS_ from sources, you need to update the master branch and rebuild/install it:
```sh
cd build
git pull
make
```
Then using root privileges:
```sh
make install
systemctl restart lms
```
# Deployment
__Note__: don't forget to give the _lms_ user read access to the music directory you want to scan.
## Configuration
_LMS_ uses a configuration file, installed by default in `/etc/lms.conf`. It is recommended to edit this file and change relevant settings (listen address, listen port, working directory, Subsonic API activation, deployment path, ...).
All other settings are set using the web interface (user management, scan settings, transcode settings, ...).
If a setting is not present in the configuration file, a hardcoded default value is used (the same as in the [default configuration file](conf/lms.conf))
## Authentication backends
You can define which authentication backend to be used thanks to the `authentication-backend` option:
* `internal` (default): _LMS_ uses an internal database to store users and their associated passwords (salted and hashed using [Bcrypt](https://en.wikipedia.org/wiki/Bcrypt)). Only the admin user can create, edit or remove other users.
* `PAM`: the user/password authentication request is forwarded to PAM (see the default [PAM configuration file](conf/pam/lms) provided).
* `http-headers`: _LMS_ uses a configurable HTTP header field, typically set by a reverse proxy to handle [SSO](https://en.wikipedia.org/wiki/Single_sign-on), to extract the login name. You can customize the field to be used using the `http-headers-login-field` option.
__Note__: the first created user is the admin user.
#### `internal` backend: reset admin password
Open the the database file located in `/var/lms/lms.db` using `sqlite3`:
```sh
sqlite3 /var/lms/lms.db
```
Now force reset the password of the admin user to `adminadmin`:
```sqlite3
UPDATE user SET password_salt="sliIgHUpEoAlBMquivH4VzKXbSzexlvS", password_hash="$2y$07$a0vnQUbGTV/DZyDqOizvbOdCaytNjDhkOPcGoBZup4V0MRKDzOpEG" WHERE id=1;
.quit
```
## Deploy on non root path
If you want to deploy on non root path (e.g. https://mydomain.com/newroot/), you have to set the `deploy-path` option accordingly in `lms.conf`.
As static resources are __not__ related to the `deploy-path` option, you have to perform the following steps if you want them to be on a non root path too:
* Create a new intermediary `newroot` directory in `/usr/share/lms/docroot` and move everything in it.
* Symlink `/usr/share/lms/docroot/newroot/resources` to `/usr/share/Wt/resources`.
* Edit `lms.conf` and set:
```
wt-resources = "" # do not comment the whole line
docroot = "/usr/share/lms/docroot/;/newroot/resources,/newroot/css,/newroot/images,/newroot/js,/newroot/favicon.ico";`
deploy-path = "/newroot/"; # ending slash is important
```
If you use nginx as a reverse proxy, you can simply replace `location /` with `location /newroot/` to achieve the same result.
## Reverse proxy settings
_LMS_ is shipped with an embedded web server, but it is recommended to deploy behind a reverse proxy. You have to set the `behind-reverse-proxy` option to _true_ in the `lms.conf` configuration file and to adjust the trusted proxy list in `trusted-proxies` option.

__Note__: when running in a docker environment, you have to trust the docker gateway IP (which is `172.17.0.1` by default)

Here is an example to make _LMS_ properly work on _myserver.org_ using _nginx_:
```
server {
    listen 80;

    server_name myserver.org;

    access_log            /var/log/nginx/myserver.access.log;

    proxy_request_buffering off;
    proxy_buffering off;
    proxy_buffer_size 4k;
    proxy_read_timeout 10m;
    proxy_send_timeout 10m;
	keepalive_timeout 10m;

    location / {
      proxy_set_header        Host $host;
      proxy_set_header        X-Real-IP $remote_addr;
      proxy_set_header        X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header        X-Forwarded-Proto $scheme;

      proxy_pass          http://localhost:5082/;
      proxy_read_timeout  120;
    }
}
```
__Note__: to mitigate brute force login attempts, _LMS_ uses an internal login throttler based on the client IP address. The `Client-IP` or `X-Forwarded-For` headers are used to determine the real IP adress, so make sure to properly configure your reverse proxy to filter or even erase the values.
# Run
```sh
systemctl start lms
```
Log traces can be accessed using journactl:
```sh
journalctl -u lms.service
```
To connect to _LMS_, just open your favorite browser and go to `http://localhost:5082`
# First launch
At the first launch, a setup assistant will guide you through creating the administrator account (username and password). Once this information is saved, you can refresh the page to access the _LMS_ login screen.
__Note__: If you are using `PAM` or `http-header` authentication, this setup assistant will not appear (see [Authentication backends](#authentication-backends)).

Once logged in as an administrator, you will need to define your libraries—that is, select the music folders you want to scan. Then, check that the scan settings meet your needs (scan frequency, tag delimiters, etc.).

You can now initiate the initial scan to import your music into _LMS_.
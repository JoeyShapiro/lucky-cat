mkdir Cert
cd Cert
& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\makecert.exe' -r -pe -ss PrivateCertStore -n CN=CatPawCert CatPawCert.cer
& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe' /driver:. /os:10_X64 /uselocaltime
& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe' sign /v /s PrivateCertStore /fd SHA256 /n CatPawCert /t http://timestamp.digicert.com CatPaw.cat

certutil -addstore "TrustedPublisher" CatPawCert.cer
certutil -addstore "Root" CatPawCert.cer


& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\makecert.exe' -r -pe -ss PrivateCertStore -n "CN=CatPawCert" -eku 1.3.6.1.5.5.7.3.3 CatPawCert.cer
certutil -addstore -f "TrustedPublisher" CatPawCert.cer
certutil -addstore -f "Root" CatPawCert.cer

& 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe' sign /v /s PrivateCertStore /fd SHA256 /n CatPawCert /tr http://timestamp.digicert.com /td sha256 CatPaw.cat
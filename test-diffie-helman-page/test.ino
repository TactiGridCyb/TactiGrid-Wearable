// // test.ino

// #include <Arduino.h>
// #include <LilyGoLib.h>        // T-Watch helper (defines `watch`)
// #include <LV_Helper.h>        // your LVGL helper (defines beginLvglHelper())
// #include <lvgl.h>


// #include "Soldier.h"
// #include "diffieHelmanPageSoldier.h"

// #include <mbedtls/x509_crt.h>
// #include <mbedtls/pk.h>

// // -----------------------------------------------------------------------------
// // 1) Your real PEM data as raw C-strings
// // -----------------------------------------------------------------------------
// // Soldier’s public cert
// static const char soldierCertPem[] = R"pem(
// -----BEGIN CERTIFICATE-----
// MIID8TCCAdmgAwIBAgIJ5nq4y4Vnb5t+MA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
// BAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
// aWRnaXRzIFB0eSBMdGQwHhcNMjUwNjEyMTYzODEwWhcNMjYwNjEyMTYzODEwWjAr
// MRcwFQYDVQQDEw5BbWl0IFJvc2VuIDIwOTEQMA4GA1UECxMHU29sZGllcjCCASIw
// DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALFuQtQM2e/B1PBLomAepOVloO5/
// E7f9SUKp0tZeldzF0FHUI71M8jFpu4UW7LEjy2OIVA9TWCd309FPi0eHsYeCIMw3
// JpFuQRKveU7K6S+pqKQXOqkA1TXcm3KdiVg/VTQfC7wYjPUi0eG51G1731v8Sfhp
// SBT9lRJRQw0LzPPf33ykXwJfgHSGk+URUO91LpuqmrStNP1RQi+JJ7YdKb8KmZx6
// OdD4xMK01TwieWLgD7cBsKZY/7++1PjN/3sKGBddjY4GxnVG/zMoi0fCH9xVI5DH
// CTn46yqbrfVj5hZ65L1LK8OBeuOjNYPJjyA+/JTbP1wMBAD1o0yw2j6HSJ8CAwEA
// ATANBgkqhkiG9w0BAQsFAAOCAgEALh+7NopyCUv2CVL6t3dD9sSyAs4aYOrGNS6Q
// ZfiabPKOqYEUYm9JuRL2A2KH4OFbQV56M0vqd/Px8TO5rKyBFV7lBSxXM4DGDsuS
// yWlBrXg75am24SgcjWxJoQ1f1HaxRLlbnoYeWbAuHimngGQzHyCKTzCAGfU1PhGK
// TjGmy1JcLFsAwPwlvEh0NZXxiwz3iaiBjWiKMPeu2Gai0CjbYiVfB8HLxg32smJ7
// Aae29ww0GWDSJTimcK3+2iGS3adVlb083scAMyhIlY+jLbHckJVt1IXu11aV2PIq
// nBSHlXIPIiy+k9wdRdOyvrB23oydFxtVB5azVEJq9C/TIpYmfhv5WDP+T5ybtqSb
// 1jnmx1Lqocl4t4YbHXvDRyMUVs/LiVMODCNw/p3XjB4aqH15ueI90t0rloOM1z5p
// RwKss+EiOip2rgYHPpsPDHcodctOgWyJRnn9a80zBxQoZJsa+XzYAniKbC55HIUz
// BgfxiF5z4rplAZI7Mnb7iQCqEFzNcWWBYObjTU1VE0fAneEKe0tQ2bCYatLRW7Ac
// TuvmwIQAdThadn6VlJjFa7gHnDNpWceacWmEqofzdwvbYkZTbkbyyR+C2u2FwBGL
// Lrut8pHB5QpNYrkR7gO9aWsigwKoQeNpXA2ejvhWrXuxkY1Dk2aKzvnMkqZPWupZ
// U8qeWxM=
// -----END CERTIFICATE-----
// )pem";

// // Soldier’s private RSA key
// static const char soldierKeyPem[] = R"pem(
// -----BEGIN RSA PRIVATE KEY-----
// MIIEpAIBAAKCAQEAsW5C1AzZ78HU8EuiYB6k5WWg7n8Tt/1JQqnS1l6V3MXQUdQj
// vUzyMWm7hRbssSPLY4hUD1NYJ3fT0U+LR4exh4IgzDcmkW5BEq95TsrpL6mopBc6
// qQDVNdybcp2JWD9VNB8LvBiM9SLR4bnUbXvfW/xJ+GlIFP2VElFDDQvM89/ffKRf
// Al+AdIaT5RFQ73Uum6qatK00/VFCL4knth0pvwqZnHo50PjEwrTVPCJ5YuAPtwGw
// plj/v77U+M3/ewoYF12NjgbGdUb/MyiLR8If3FUjkMcJOfjrKput9WPmFnrkvUsr
// w4F646M1g8mPID78lNs/XAwEAPWjTLDaPodInwIDAQABAoIBAAEKfRDibGroL7gj
// MXYv59bCtGGB1FtJfKML/QUt8+IiNPkt7fG/4FHi5Ws/+a6GX7ybQab/Xa6JmPAK
// 7+3BAY0iec/I6QyEYZRa/DK2pQyXCbrAuiLgst+Ihk7zVqlyYa0XG4oEeNs9U47n
// +LOVvPc8sTBoabhZzoSXfnW2ooKxxIgZ86z0fPFoP78n1n23EHD0WQbVFn3MfZNU
// PkkzB4cTUEzPeZAovOzUvxt+9zyeuS1P30nEkYSOCOotXSm0VQUA3bpZKHrhqMGm
// 3h13aA2EWA9RnzlzHC+r4RSINBgYr/suTmnAA8pYGZpIf7WXCdK4O4xjUxoT0jF0
// +A01bSUCgYEA8b+hHla0k5GjaWv+ZTR0mLZROq+JuZbBV4NfHwa9JVpa5G9zPsk8
// ITS59/Ismo33XuIEyDSg1mH+al4ekqw1u7BzM/qOczgtDwOliArr1wE6oo9su8ZO
// gwN9DrHVuRzOx/rN7QYmDwxUNfaXg8i+pwdiqdV+r0vz1pAUHyG7rtMCgYEAu+P5
// FMovfMTMbR3NlHm3k42aotAhmhpXExLRZ+6/vb+sdizYIDnN5MHMTKYf/mZpqHxn
// XofJSD5si8DIf7zbM+XTjwe6Wok+IenaYzkpKfJ5qYK60JESLl+rRXCkECVtebn5
// r+NbD7jBN+OujceGDSQ80QjM6+ioZwRpIDIdl4UCgYA6rEnMdTrKfhRtJ7rMkVij
// H8zDM4t3sbEnLklN3HLXuABLZQlRecHQRV3FYc+Vi1M4gh6rAKrwnUQESOeerZE1
// BnRPb0ZXjJDTDg2E4TGwMyop/iljwZOYlKYrhNncXbOKMcL8/fsKt/FQT5Midvxe
// yZoWoixnG0YJE1ru218T5QKBgQCOawMGaysLm/CIVSra/FfWFGO+PZz6vjR6VK09
// 5o6YOY10FhHMe4RBfVRqVRGzN9WmzIVd9fbwN8D3Pa28hV5yPCbcJ7NtpNHfyu6q
// f01galcl0d3g8dWiKQodnH//bR9KQVmtpNwFUrnCr2ZEOZS2qV0f5VfPCY98Zq94
// pAQiMQKBgQCVptEgXE68CaLWZf7uU8itsX3AzUNkGrhOg+8sFYSpfwTPJHVPlMFO
// HEL8uSZhPM/REvulXUgRbglI6WIlxXs9VYxiXLFZpgurU8DfUReVvY0IhZRII/VE
// 1hrJkqDXH50kdNVd2BCRn2GTKrwn5mhTtsIkyKEZM+pkYoUX2V3wSA==
// -----END RSA PRIVATE KEY-----
// )pem";

// // CA’s public cert
// static const char caCertPem[] = R"pem(
// -----BEGIN CERTIFICATE-----
// MIIFazCCA1OgAwIBAgIUc/1MdZLW+IXLILwGW4bT692B4fkwDQYJKoZIhvcNAQEL
// BQAwRTELMAkGA1UEBhMCSUwxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
// GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yNTA1MDQyMDE1NDFaFw0zNTA1
// MDIyMDE1NDFaMEUxCzAJBgNVBAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw
// HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggIiMA0GCSqGSIb3DQEB
// AQUAA4ICDwAwggIKAoICAQDAEODmLoHm0kVyg7/AV7LYGHXlshnfqZ4hpA2dU0FA
// JwNVHHCHm0bV7jqtf4DxZoDYekLpc5Z7yd/J5binPmCtD8nTzdrwkUhN3U74nzhs
// Ln22UidEGiWdrG74vTFsecD2n9EDmR6w5RPgt3ktDF4g1Bp+PbNzp8tEsk9Lbw/I
// QDYKx8hNJ3mFcBZqfq0az8mU85K1Evx7VBw733yhvGxAv7Cv6HUq4H/zHNWGLzYK
// rAwUf60TC+D+JYRgGAmKjW17w3bs02eARY5i9pI/Re64fSqVLJ/t06lS3GJTzFFU
// HZ/y4Bitveo2MVy7hgT1oOpS8LIcQC7snK+kY9MaiwUPFPdlfP7gMbwd8ym7zuqQ
// yK5pJbpu88lg1CekDALRr1TUjPj0mbiVS/dZEwXpkqlThap5kgLi3r9M4zTT1q5h
// EX/ZKNBKTQQupl3AriXcIv+3XPy64fC/S5GMbt5UQRZXHRQlXMa4Stc+MFuLlyar
// SUoKJpjqLGGCdOBl/LMPi+QIx3upXjaI9fQ3/jRiP4rgRgH3hvoH8nRexGfdnXkV
// pHrQjG934qgfomq86/HU1/RR7cnP0syrTItTpu/kn8RTrw39iy6j08sWUIESXU4R
// OQ1x4GYxjPW3ELvfmlvrXhmyW9qoKaKLUATnDXb6s72EyScpPIs74iiu33ItD359
// PwIDAQABo1MwUTAdBgNVHQ4EFgQUSc9uhthjOXXX5SecylqTgjl5mwswHwYDVR0j
// BBgwFoAUSc9uhthjOXXX5SecylqTgjl5mwswDwYDVR0TAQH/BAUwAwEB/zANBgkq
// hkiG9w0BAQsFAAOCAgEAcVAZMGgfjZD2XLWVkFwe+gxl/1BQGVw5S5e1Fpqa/wfy
// MdKugIoH5Bh/sP0GIxrjDHaLXcu22sJie0cuNZsPZC4Uby/LhE7AbyE+WClD9hym
// uTQJzq0dQyuNfGj9e6XBhuVD2//wztIkTykzOmzCX+bhD5K4ibGEiauEEzBG14uy
// Pk9jhMZ95lI9WACSnQH1UdH9AkMZ2IyWuvVzvg7nw08BxA/FpfAviKvzfjCInnmP
// BJmGpx6otnQy3q1bBGYe68KYV0a1DuaG5i/52hZv7/4oS4F210pwUfvvriDxLVdv
// TGlBKnFD2gdLfRD7JoFoPgFD4SDcaS29jDGoIlJHFhXDxYG/OEyIBwJcuAbcYztb
// YGAEyCYtkGL0EdKnunVGfAf8D4O7YCCykFkVGhvq+QvGGsbYhw+Uil7o6sdYzmF8
// 7JVBKDn0dCFr7N09X6EsbDxg5sZQAe1DMzdYp06d+xqLAlz10VOF2fvxCnH6+xid
// /KQjvtd1dduBueqsKPykXFt4ig1ZluwyHR0RYdd4W62QtajaMk21qbgTiQcZLqvI
// cdQxhfrZnIKMvI6J63rC4WUz8RzGZZ8JghPEDLD0i01nEWmtd+VhOe+mUaIDMCr9
// XHBePYiPBuUgVC9lDYPNQzcTS9BogzpQzO0jbCivx9lwPXJmHRcp7RAk28C99IY=
// -----END CERTIFICATE-----
// )pem";

// // Commander’s public cert (first element of "commanders" array)
// static const char commanderCertPem[] = R"pem(
// -----BEGIN CERTIFICATE-----
// MIID9zCCAd+gAwIBAgIJsf9GLo4x6F6CMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
// BAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
// aWRnaXRzIFB0eSBMdGQwHhcNMjUwNjEyMTYzODEyWhcNMjYwNjEyMTYzODEyWjAx
// MRswGQYDVQQDExJTaGlyYSBCZW4tRGF2aWQgNzMxEjAQBgNVBAsTCUNvbW1hbmRl
// cjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMRowuGEelu6rVlni2hG
// gJSJhYS4mDCsOsPcrVu5ABelfcr+hTMPguajanZsG9WNQkRdx2JcwciCU7HEp7Bi
// hyh6l+a036e4woHqjqYzxIy92k7VHOjUq/73Iz4eabZsGPERrKjj4uQmwNBr7cem
// m2TXxGKDHlDHHWikEJtHS7E9zrjc6YjwBnMpYZ0jGBiOfmYMmUkrGylQ7GTT/1dp
// H6odOQ3AfwxTUioFmPP69ZAxLkspHtjTo/176h5LC6Y3lP4m/Q4Gfq1/Jpp0GU8l
// 9QUzBBBNRCqPwAibBcf6oijjmF2kmQY3HTp6vAGPYmrHRegzlIUoJoid3ibA7MKk
// wYkCAwEAATANBgkqhkiG9w0BAQsFAAOCAgEAljv24EAJVl+moB67+FPzrUsvZTnT
// SJ8Ioes55CR91y66iYGD1RVvja0BYxwOGcl3SlZ3t0NR3iYYZ/ZBj3+yTU0bGBh4
// 1iT+SVcwtEbgaPYKBdGr/dCUYngXbowYXFM9ukl6nenHeTpej+etQWePL43lfK+2
// Cpjj6Sz/0H6z2SXaiVSXNeaTBFsm7YFJ7asrCzVXUWK8dSIdrUmh8xjPAn9BIUzB
// IUP8o5tyjdxOWV+eopjwbRGFyh/pu60zUfWr/2uBF7DmjPfnJxym0puV4+if6sWP
// 0JAjyC1zelgcQR9Lsx2z9I7aKBEI+OF0C32PegQpCrJFfamLfYfVjXzAlF+uQZl2
// NWBls3kfsQVx6l+V37iYRNItXyGM4jEANN5IqHcVSVj5kHBKD5y0cBj99Aci5p2G
// XEHMi1ZdXgBGaTR8ucdlIDB99ijy3QUTLa7lnEDjIKyGgtTq8CsiLjsiV/wUHI48
// acDQJ14tsrVrYurDuEaDblIdXiFvF451bMAoi453hhMvwQQLzlHHi3BOBFMDEmQf
// SjYJDhAjLmoQwC2VQbjHTb+UxOLQIivVPg8S7V/MdAF6uUeuGq5ax3P/0x7s+3Vx
// fuNyVwbkzr9fEP6wNE5UGHW3w0oUYh/3+/SnSP501LVbF8j+4Zc8XSV75l+mpS1a
// v71K4hP65L79eCw=
// -----END CERTIFICATE-----
// )pem";

// // -----------------------------------------------------------------------------
// // 2) Helpers: parse PEM → mbedtls structures
// // -----------------------------------------------------------------------------
// bool parseCertificate(const char* pem, mbedtls_x509_crt& crt) {
//   mbedtls_x509_crt_init(&crt);
//   int ret = mbedtls_x509_crt_parse(
//     &crt,
//     reinterpret_cast<const unsigned char*>(pem),
//     strlen(pem) + 1
//   );
//   if (ret != 0) {
//     Serial.printf("crt_parse failed: -0x%04X\n", -ret);
//     return false;
//   }
//   return true;
// }

// bool parsePrivateKey(const char* pem, mbedtls_pk_context& key) {
//   mbedtls_pk_init(&key);
//   int ret = mbedtls_pk_parse_key(
//     &key,
//     reinterpret_cast<const unsigned char*>(pem),
//     strlen(pem) + 1,
//     nullptr, 0
//   );
//   if (ret != 0) {
//     Serial.printf("pk_parse failed: -0x%04X\n", -ret);
//     return false;
//   }
//   return true;
// }

// // -----------------------------------------------------------------------------
// // 3) Globals
// // -----------------------------------------------------------------------------
// Soldier* soldier = nullptr;
// DiffieHellmanPageSoldier* dhPage = nullptr;

// // -----------------------------------------------------------------------------
// // 4) Arduino setup & loop
// // -----------------------------------------------------------------------------
// void setup() {
//   Serial.begin(115200);
//   while (!Serial) { delay(10); }
//   delay(5000);

//   // a) Core LVGL + T-Watch init
//   lv_init();
//   watch.begin();
//   beginLvglHelper();

//   // 4.3) Parse the PEM blobs
//   mbedtls_x509_crt cert, caCert;
//   mbedtls_pk_context key;
//   if (!parseCertificate(soldierCertPem, cert)
//    || !parseCertificate(caCertPem,   caCert)
//    || !parsePrivateKey(  soldierKeyPem, key)) {
//     Serial.println("❌ PEM parsing failed, halting.");
//     while (true) delay(1000);
//   }

//   uint8_t soldierNumber=1, intervalMS=1000;

//   // 4.4) Instantiate Soldier + page
//   soldier = new Soldier(
//     "Soldier1",
//     cert,
//     key,
//     caCert,
//     soldierNumber=1,
//     intervalMS=1000
//   );
//   //add commander
//   // 1) Parse the raw PEM you already have into an mbedtls_x509_crt:
//     mbedtls_x509_crt cmdCert;
//     if (! parseCertificate(commanderCertPem, cmdCert) ) {
//     Serial.println("❌ failed to parse commander PEM");
//     return;
//     }

//     // 2) Fill out a CommanderInfo struct:
//     CommanderInfo cmdInfo;
//     cmdInfo.name                 = "Commander1";            // any label you like
//     cmdInfo.commanderNumber      = 10;                       // unique ID
//     cmdInfo.cert                 = cmdCert;                 // the parsed certificate
//     cmdInfo.lastTimeReceivedData = millis();                // now()
//     cmdInfo.status               = REGULAR;                 // initial status

//     // 3) Add it to your soldier’s internal map:
//     soldier->addCommander(cmdInfo);

//     dhPage = new DiffieHellmanPageSoldier(soldier);
//     dhPage->createPage();
// }

// void loop() {
//   lv_timer_handler();  // drives LVGL (and your page.poll())
//   delay(5);
// }



































// test_commander.ino

#include <Arduino.h>
#include <LilyGoLib.h>               // TTGOClass watch helper
#include <LV_Helper.h>               // beginLvglHelper()
#include <lvgl.h>

#include "Commander.h"
#include "diffieHelmanPageCommander.h"

#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>

// -----------------------------------------------------------------------------
// 1) PEM blobs exactly as in your JSON (no "\r\n" escapes, full base64)
// -----------------------------------------------------------------------------

// Commander’s public certificate
static const char commanderCertPem[] = R"pem(
-----BEGIN CERTIFICATE-----
MIID9zCCAd+gAwIBAgIJsf9GLo4x6F6CMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
BAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
aWRnaXRzIFB0eSBMdGQwHhcNMjUwNjEyMTYzODEyWhcNMjYwNjEyMTYzODEyWjAx
MRswGQYDVQQDExJTaGlyYSBCZW4tRGF2aWQgNzMxEjAQBgNVBAsTCUNvbW1hbmRl
cjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMRowuGEelu6rVlni2hG
gJSJhYS4mDCsOsPcrVu5ABelfcr+hTMPguajanZsG9WNQkRdx2JcwciCU7HEp7Bi
hyh6l+a036e4woHqjqYzxIy92k7VHOjUq/73Iz4eabZsGPERrKjj4uQmwNBr7cem
m2TXxGKDHlDHHWikEJtHS7E9zrjc6YjwBnMpYZ0jGBiOfmYMmUkrGylQ7GTT/1dp
H6odOQ3AfwxTUioFmPP69ZAxLkspHtjTo/176h5LC6Y3lP4m/Q4Gfq1/Jpp0GU8l
9QUzBBBNRCqPwAibBcf6oijjmF2kmQY3HTp6vAGPYmrHRegzlIUoJoid3ibA7MKk
wYkCAwEAATANBgkqhkiG9w0BAQsFAAOCAgEAljv24EAJVl+moB67+FPzrUsvZTnT
SJ8Ioes55CR91y66iYGD1RVvja0BYxwOGcl3SlZ3t0NR3iYYZ/ZBj3+yTU0bGBh4
1iT+SVcwtEbgaPYKBdGr/dCUYngXbowYXFM9ukl6nenHeTpej+etQWePL43lfK+2
Cpjj6Sz/0H6z2SXaiVSXNeaTBFsm7YFJ7asrCzVXUWK8dSIdrUmh8xjPAn9BIUzB
IUP8o5tyjdxOWV+eopjwbRGFyh/pu60zUfWr/2uBF7DmjPfnJxym0puV4+if6sWP
0JAjyC1zelgcQR9Lsx2z9I7aKBEI+OF0C32PegQpCrJFfamLfYfVjXzAlF+uQZl2
NWBls3kfsQVx6l+V37iYRNItXyGM4jEANN5IqHcVSVj5kHBKD5y0cBj99Aci5p2G
XEHMi1ZdXgBGaTR8ucdlIDB99ijy3QUTLa7lnEDjIKyGgtTq8CsiLjsiV/wUHI48
acDQJ14tsrVrYurDuEaDblIdXiFvF451bMAoi453hhMvwQQLzlHHi3BOBFMDEmQf
SjYJDhAjLmoQwC2VQbjHTb+UxOLQIivVPg8S7V/MdAF6uUeuGq5ax3P/0x7s+3Vx
fuNyVwbkzr9fEP6wNE5UGHW3w0oUYh/3+/SnSP501LVbF8j+4Zc8XSV75l+mpS1a
v71K4hP65L79eCw=
-----END CERTIFICATE-----
)pem";


// Commander’s private RSA key
static const char commanderKeyPem[] = R"pem(
-----BEGIN RSA PRIVATE KEY-----
MIIEogIBAAKCAQEAxGjC4YR6W7qtWWeLaEaAlImFhLiYMKw6w9ytW7kAF6V9yv6F
Mw+C5qNqdmwb1Y1CRF3HYlzByIJTscSnsGKHKHqX5rTfp7jCgeqOpjPEjL3aTtUc
6NSr/vcjPh5ptmwY8RGsqOPi5CbA0Gvtx6abZNfEYoMeUMcdaKQQm0dLsT3OuNzp
iPAGcylhnSMYGI5+ZgyZSSsbKVDsZNP/V2kfqh05DcB/DFNSKgWY8/r1kDEuSyke
2NOj/XvqHksLpjeU/ib9DgZ+rX8mmnQZTyX1BTMEEE1EKo/ACJsFx/qiKOOYXaSZ
BjcdOnq8AY9iasdF6DOUhSgmiJ3eJsDswqTBiQIDAQABAoIBABbC+Uq/WprE0JAa
4toLyZztLw8JgYGqhAjsyx5lGzCiWiirRG59brMh3xWoQ7WE8FgR8ihhDYgaKm7g
lnpngLhdNLtk3MKIM+pwb2WtfCq/WcxnADmvY9thcrEhPykH8AAIB1VvS30bTJ8a
0uyfT0TpiHXOMjY0TSEyWkZUhd4dUpfWBzF6eSwKV1gw5fgMyWfNbnIkvaM5ZXwe
YapPtE/vVhdVUrkPlQtFjwcyaNR7luteyb5P4fbq5V9rd3IHaHEgKvnQJgkuJtJh
O15hUHysX0iefwkZzpkqnHj0/XOwcBXDF43d3AjoGRl7n8ZEH1jwaDh8BUuooH8R
b8ZuaV0CgYEA5uxGUi0wtSVRDbEikb+aKqm6bGWhuHKluUvDg+XAKAM+0cLX2oVD
Lt6cYHIsTpq6QhJ9pJv5SzsOo7XbypptRa2WOmb0Cjoto3ohwQ40bb7SpywabndA
QG1dHvZS3gOuTR9bvVgVvwhpqsh0+0KrA1QMkwyApRi+Sd0yL19Azh8CgYEA2bz+
tZNxB4mFkM+21lWgLjv4+tlifu1OP0hCAREC6KGFc5Kn22wfEPaV3yA/B0L5n19G
BHbXK0M5QZxqTcU2y3iWje+9JixTkXeekDgoIyI21om88QQkzXgUOBDfeBSyHcRC
8ufBGxgTFuZc/4NkCLPpfHg1P5XlkuHHIsgQq1cCgYByjsRUABcpxllvcXC03Vid
0ZWqMMEJv9Z3Fh7oUBIDx5hid0aeIX4ywjzRm9JfLGM6Y/Hwt2/04ldg39cpq3KK
HpNoZaqraDE76FrWazWXPBNE25xBMOevDpIjyg9SFIFjwSrBw/EF1CaXktp1y50L
CaczTACF5sB/5DwBRD/iAQKBgEQCSjSAxw8pnzRqDJvJxuxqAwynFaK7kHMnqKYY
oCX1PW+p4RAiJ1nvC0TUF5u3Ca0D3yTJ0c9LgfjCWFnOPZ00HyJaPWRM+BU5nadC
QxcmOqasAv3s42niFb6lVod1P2UYxiiExsYlsOC4N1f/vCETwNwF1+vyb/B+4Oo2
Py6lAoGAXCEuS7roUXMatkzdKtWB3SBJ4GY5M1wvLAWJOCYlYx0YC1LVLW92JkLK
VCIJwvT/wI9iQmmk2K3MwxEHRvKWQFZVHMMLK2BZ1pItp4+VgbzsnUfwKC1CzQyk
mb2sy/zaQ08Um6RG71lZOVb2/s04rKm77E98mMLCPqFVaLsL3Do=
-----END RSA PRIVATE KEY-----
)pem";

// CA’s public certificate
static const char caCertPem[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIUc/1MdZLW+IXLILwGW4bT692B4fkwDQYJKoZIhvcNAQEL
BQAwRTELMAkGA1UEBhMCSUwxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yNTA1MDQyMDE1NDFaFw0zNTA1
MDIyMDE1NDFaMEUxCzAJBgNVBAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw
HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQDAEODmLoHm0kVyg7/AV7LYGHXlshnfqZ4hpA2dU0FA
JwNVHHCHm0bV7jqtf4DxZoDYekLpc5Z7yd/J5binPmCtD8nTzdrwkUhN3U74nzhs
Ln22UidEGiWdrG74vTFsecD2n9EDmR6w5RPgt3ktDF4g1Bp+PbNzp8tEsk9Lbw/I
QDYKx8hNJ3mFcBZqfq0az8mU85K1Evx7VBw733yhvGxAv7Cv6HUq4H/zHNWGLzYK
rAwUf60TC+D+JYRgGAmKjW17w3bs02eARY5i9pI/Re64fSqVLJ/t06lS3GJTzFFU
HZ/y4Bitveo2MVy7hgT1oOpS8LIcQC7snK+kY9MaiwUPFPdlfP7gMbwd8ym7zuqQ
yK5pJbpu88lg1CekDALRr1TUjPj0mbiVS/dZEwXpkqlThap5kgLi3r9M4zTT1q5h
EX/ZKNBKTQQupl3AriXcIv+3XPy64fC/S5GMbt5UQRZXHRQlXMa4Stc+MFuLlyar
SUoKJpjqLGGCdOBl/LMPi+QIx3upXjaI9fQ3/jRiP4rgRgH3hvoH8nRexGfdnXkV
pHrQjG934qgfomq86/HU1/RR7cnP0syrTItTpu/kn8RTrw39iy6j08sWUIESXU4R
OQ1x4GYxjPW3ELvfmlvrXhmyW9qoKaKLUATnDXb6s72EyScpPIs74iiu33ItD359
PwIDAQABo1MwUTAdBgNVHQ4EFgQUSc9uhthjOXXX5SecylqTgjl5mwswHwYDVR0j
BBgwFoAUSc9uhthjOXXX5SecylqTgjl5mwswDwYDVR0TAQH/BAUwAwEB/zANBgkq
hkiG9w0BAQsFAAOCAgEAcVAZMGgfjZD2XLWVkFwe+gxl/1BQGVw5S5e1Fpqa/wfy
MdKugIoH5Bh/sP0GIxrjDHaLXcu22sJie0cuNZsPZC4Uby/LhE7AbyE+WClD9hym
uTQJzq0dQyuNfGj9e6XBhuVD2//wztIkTykzOmzCX+bhD5K4ibGEiauEEzBG14uy
Pk9jhMZ95lI9WACSnQH1UdH9AkMZ2IyWuvVzvg7nw08BxA/FpfAviKvzfjCInnmP
BJmGpx6otnQy3q1bBGYe68KYV0a1DuaG5i/52hZv7/4oS4F210pwUfvvriDxLVdv
TGlBKnFD2gdLfRD7JoFoPgFD4SDcaS29jDGoIlJHFhXDxYG/OEyIBwJcuAbcYztb
YGAEyCYtkGL0EdKnunVGfAf8D4O7YCCykFkVGhvq+QvGGsbYhw+Uil7o6sdYzmF8
7JVBKDn0dCFr7N09X6EsbDxg5sZQAe1DMzdYp06d+xqLAlz10VOF2fvxCnH6+xid
/KQjvtd1dduBueqsKPykXFt4ig1ZluwyHR0RYdd4W62QtajaMk21qbgTiQcZLqvI
cdQxhfrZnIKMvI6J63rC4WUz8RzGZZ8JghPEDLD0i01nEWmtd+VhOe+mUaIDMCr9
XHBePYiPBuUgVC9lDYPNQzcTS9BogzpQzO0jbCivx9lwPXJmHRcp7RAk28C99IY=
-----END CERTIFICATE-----
)pem";

// Soldier’s public certificate
static const char soldierCertPem[] = R"pem(
-----BEGIN CERTIFICATE-----
MIID8TCCAdmgAwIBAgIJ5nq4y4Vnb5t+MA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV
BAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
aWRnaXRzIFB0eSBMdGQwHhcNMjUwNjEyMTYzODEwWhcNMjYwNjEyMTYzODEwWjAr
MRcwFQYDVQQDEw5BbWl0IFJvc2VuIDIwOTEQMA4GA1UECxMHU29sZGllcjCCASIw
DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALFuQtQM2e/B1PBLomAepOVloO5/
E7f9SUKp0tZeldzF0FHUI71M8jFpu4UW7LEjy2OIVA9TWCd309FPi0eHsYeCIMw3
JpFuQRKveU7K6S+pqKQXOqkA1TXcm3KdiVg/VTQfC7wYjPUi0eG51G1731v8Sfhp
SBT9lRJRQw0LzPPf33ykXwJfgHSGk+URUO91LpuqmrStNP1RQi+JJ7YdKb8KmZx6
OdD4xMK01TwieWLgD7cBsKZY/7++1PjN/3sKGBddjY4GxnVG/zMoi0fCH9xVI5DH
CTn46yqbrfVj5hZ65L1LK8OBeuOjNYPJjyA+/JTbP1wMBAD1o0yw2j6HSJ8CAwEA
ATANBgkqhkiG9w0BAQsFAAOCAgEALh+7NopyCUv2CVL6t3dD9sSyAs4aYOrGNS6Q
ZfiabPKOqYEUYm9JuRL2A2KH4OFbQV56M0vqd/Px8TO5rKyBFV7lBSxXM4DGDsuS
yWlBrXg75am24SgcjWxJoQ1f1HaxRLlbnoYeWbAuHimngGQzHyCKTzCAGfU1PhGK
TjGmy1JcLFsAwPwlvEh0NZXxiwz3iaiBjWiKMPeu2Gai0CjbYiVfB8HLxg32smJ7
Aae29ww0GWDSJTimcK3+2iGS3adVlb083scAMyhIlY+jLbHckJVt1IXu11aV2PIq
nBSHlXIPIiy+k9wdRdOyvrB23oydFxtVB5azVEJq9C/TIpYmfhv5WDP+T5ybtqSb
1jnmx1Lqocl4t4YbHXvDRyMUVs/LiVMODCNw/p3XjB4aqH15ueI90t0rloOM1z5p
RwKss+EiOip2rgYHPpsPDHcodctOgWyJRnn9a80zBxQoZJsa+XzYAniKbC55HIUz
BgfxiF5z4rplAZI7Mnb7iQCqEFzNcWWBYObjTU1VE0fAneEKe0tQ2bCYatLRW7Ac
TuvmwIQAdThadn6VlJjFa7gHnDNpWceacWmEqofzdwvbYkZTbkbyyR+C2u2FwBGL
Lrut8pHB5QpNYrkR7gO9aWsigwKoQeNpXA2ejvhWrXuxkY1Dk2aKzvnMkqZPWupZ
U8qeWxM=
-----END CERTIFICATE-----
)pem";

// -----------------------------------------------------------------------------
// 2) Helpers: parse PEM → mbedtls structures
// -----------------------------------------------------------------------------
bool parseCertificate(const char* pem, mbedtls_x509_crt& crt) {
  mbedtls_x509_crt_init(&crt);
  int ret = mbedtls_x509_crt_parse(
    &crt,
    reinterpret_cast<const unsigned char*>(pem),
    strlen(pem) + 1
  );
  if (ret != 0) {
    Serial.printf("crt_parse failed: -0x%04X\n", -ret);
    return false;
  }
  return true;
}

bool parsePrivateKey(const char* pem, mbedtls_pk_context& key) {
  mbedtls_pk_init(&key);
  int ret = mbedtls_pk_parse_key(
    &key,
    reinterpret_cast<const unsigned char*>(pem),
    strlen(pem) + 1,
    nullptr, 0
  );
  if (ret != 0) {
    Serial.printf("pk_parse failed: -0x%04X\n", -ret);
    return false;
  }
  return true;
}


// -----------------------------------------------------------------------------
// 3) Globals
// -----------------------------------------------------------------------------
Commander* commander = nullptr;
DiffieHellmanPageCommander* dhPage = nullptr;

// -----------------------------------------------------------------------------
// 4) setup() / loop()
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while(!&Serial){ delay(10); }
  delay(5000);

  lv_init();
  watch.begin();
  beginLvglHelper();

  // parse our three PEM blobs
  mbedtls_x509_crt cert, caCert;
  mbedtls_pk_context key;
  if (!parseCertificate(commanderCertPem, cert) ||
      !parseCertificate(caCertPem,      caCert) ||
      !parsePrivateKey(commanderKeyPem, key)) {
    Serial.println("❌ PEM parse failed; halting.");
    while (true) delay(1000);
  }

  // build Commander + page
  commander = new Commander("Commander1", cert, key, caCert, 10, 1000);

  // add one soldier entry so our poll() sees someone to talk to
  mbedtls_x509_crt sldCert;
  if (!parseCertificate(soldierCertPem, sldCert)) {
    Serial.println("❌ soldier cert parse failed");
    while (true) delay(1000);
  }
  SoldierInfo si{};
  si.name                 = "Soldier1";
  si.soldierNumber        = 1;
  si.cert                 = sldCert;
  si.lastTimeReceivedData = millis();
  si.status               = REGULAR;
  commander->addSoldier(si);

  dhPage = new DiffieHellmanPageCommander(commander);
  dhPage->createPage();
}

void loop() {
  lv_timer_handler();  // runs LVGL & your DH page.poll()
  delay(5);
}

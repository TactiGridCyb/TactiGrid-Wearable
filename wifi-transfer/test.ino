#include <LilyGoLib.h>
#include <string>
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"

String replaceAll(const String& str, const String& from, const String& to) {
  String result = str;
  int index = result.indexOf(from);
  while (index != -1) {
    result.remove(index, from.length());
    result = result.substring(0, index) + to + result.substring(index);
    index = result.indexOf(from, index + to.length());
  }
  return result;
}

String extractPemBlock(const String& pem, const String& beginMarker, const String& endMarker) {
  int startIndex = pem.indexOf(beginMarker);
  if (startIndex == -1) return "";
  int endIndex = pem.indexOf(endMarker, startIndex);
  if (endIndex == -1) return "";
  endIndex += endMarker.length();
  return pem.substring(startIndex, endIndex);
}

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);

  String combinedPem = "-----BEGIN CERTIFICATE-----\r\nMIIEDjCCAfagAwIBAgIJvYkFwQ3HaIG8MA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\r\nBAYTAklMMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\r\naWRnaXRzIFB0eSBMdGQwHhcNMjUwNTI2MTkwMjEwWhcNMjYwNTI2MTkwMjEwWjBI\r\nMQ8wDQYDVQQDEwZQI2I0NmYxITAfBgNVBAUTGDY4MWRmYzIyOWVkOTA4NDE0OTkw\r\nYjQ2ZjESMBAGA1UECxMJQ29tbWFuZGVyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A\r\nMIIBCgKCAQEAwelIycgwYCaLFQX5eM6cTY1iyJZay7Zm1WIchTas59Xam0wtkj+K\r\nZTCIIz/F+fMtFolNTQEeQrWF28lbJ2JCW8JYt8ZvUt3yakP46UDxtygFlQX6nrPT\r\nKXcXthuPs6DhPNdTOIvBu5I7n/hdKGxhls0wGdnFDSZI8ikcrebNjVgW1iECFpjB\r\nuRmiPkCTGJtyG7KNDZmU1UFGPG5Mjb+oziEtkfqPD+MV0WQnRwwI7n4uu1c1k/aU\r\n73xCwdbA6XVic/lUa2wXYv3gN9i1KldmAoYDtsIUwZD0nj2DDxAceut18E5wM8Kn\r\n0o3vajkdUlP+MgReGg+oUdPib+7cegwkjQIDAQABMA0GCSqGSIb3DQEBCwUAA4IC\r\nAQBjNFjUIn5USnSqLTNva7n05Ng3IO0kqUr1nJ/oiyhoya257VuZdnMw1+mH5agV\r\nWQK7a9ae58tczWRszTonOiDLXlMP2fsjfciXDiIf179I97yX60uXyDqg3iIC+Hdb\r\n3XsjlMC1GCyhhaxgP2n29txi7zeJPKqAcUGAbB3gsllzDRAhm1GR8it9c42gzyP+\r\nRin5xkN75TTFs7Di9vosWwW388X22W1PU2nzpjREXuy/1bDLXwnqgHVAVEnLWmqM\r\ndcdYdf/uWEbUAcX9oil0E8f81VoTMWMDN84WmRbSqGaMh1dCq/Pn28ZX9TzaBPHZ\r\nsNHKjcZSqrIDBrCbLMp6h/tr2/lj2sRU4wV3VaaNh1IBjXV5Ak8SC4v4vII55fYz\r\ntIDRruicKEBtHIIUg3DMjJbKf/5o64ST+1cVeou88z7VXvQFwlVxEMcZEHLk6szy\r\niM4RoBHTuNgrNA0RmpQ37+8JHgqPzNbzX4f+u0H3CVswd7tyueQ3yAreTQ1D8O3C\r\n420Z1zdopB5IlNUtYE2LzgzWaIVgmiCu7WSUBeVbknOUBiLEsfyhl1vUFQUyM99C\r\nu9WenRUYBtI1nrFTjol09b/6Ql/GuqCGxpRZX0ZKvum5zGFAikf/Y4eQWS6ZMxmT\r\nGyIotWY9MeOsjopVWiDnmpTTLVf6Qg2H6AJj7i6Zu8+QRA==\r\n-----END CERTIFICATE-----\r\n-----BEGIN RSA PRIVATE KEY-----\r\nMIIEpQIBAAKCAQEAwelIycgwYCaLFQX5eM6cTY1iyJZay7Zm1WIchTas59Xam0wt\r\nkj+KZTCIIz/F+fMtFolNTQEeQrWF28lbJ2JCW8JYt8ZvUt3yakP46UDxtygFlQX6\r\nnrPTKXcXthuPs6DhPNdTOIvBu5I7n/hdKGxhls0wGdnFDSZI8ikcrebNjVgW1iEC\r\nFpjBuRmiPkCTGJtyG7KNDZmU1UFGPG5Mjb+oziEtkfqPD+MV0WQnRwwI7n4uu1c1\r\nk/aU73xCwdbA6XVic/lUa2wXYv3gN9i1KldmAoYDtsIUwZD0nj2DDxAceut18E5w\r\nM8Kn0o3vajkdUlP+MgReGg+oUdPib+7cegwkjQIDAQABAoIBAAi9VKpBg0qmGhEj\r\nwIttFnbZVzkuq59wVGCsKBhp7y+QwCZNL+WeRwTKA+zAyFG8X80a+ZitovlDOKoA\r\nAIN9Jnv/vaNkxmov9ifLcfnDU1GbtGqzxwillrAQ/cjXo6mnJ9en5ciP5fan/9d1\r\nKG+0uM5E57TWj47I6bM+XpxI4X1MBZIMUqynR7R/vj/VVyEDrB7gkZlRN6BnjBdS\r\nW3i0nBhVUkmS18qw8n26s1ORL3MRKyryqj8JBB5QgT7yCzwh7ykm25Z+GQVwpVNX\r\nWchQisAnTmBcZvxAi1G63Af/TC88B6pPgw7nET3Rw9oC+QNEMDo9z8oGx2P9NRjs\r\nKOHf6LECgYEA/qg6fpu7FpiEXP6S5rvjZq199hK7IAwNqkWUnVJtK+/Pt0VfProO\r\nOimjfdN2XbjxzyvQC4SBYxRDKJ5l+TpAKBQvhy8047wVb1unyno76RXiCl+AUO1A\r\npqU+5Eu48X0zqvNaXDqUh3Etjl9z2WT8rLGIIMaFrDQ2w+F3161exZ0CgYEAwu8N\r\neVHrItvQudWQxbQ1E0qcdxhwbyRRytIKvFXFYq1xoLCRkz29AhPxXYcKeeTe1zKh\r\n/kvuLArhbkFnL/nQ0VLDvFj+1/7oBx/dW0zzdnNXdsI/dboM3pmqEreywwc7pccC\r\nCw3GZGz9WCf4xD4DUILjEfvjem9VpyLoUAwdn7ECgYEAifE+nk5lLXw4VtdfY7Tr\r\nHTdlP+ItktJ0pINEWWPI1z/z2GavjR7jjgX4FbRyLZp9AafN7i88lxrmth33RuWS\r\n0yL8C9I6aBH92nBfl4JW6Y98/fl1XGDn4F0qkCekastTLYrcq5Df2+4mqzRxYJUf\r\nebxA7OAckIdIExsS+7Vh3NECgYEAmQ6eY73ghFRYRTHdJH7klaslw5Va4sl/t2LI\r\nrywheeN5rpQ1GYmGWJVUFP2tShxcpFfLPiJcdhRtAOc1oEPkpBb6PW8bWnl5se5h\r\nHgkDuOPDrynCLivRYw8ArQWzxNJvETF69zbvqXayTX/FIkEW1SaQKFTBPicg98S+\r\n5+s7b2ECgYEA6QwRGus5XhXbjW7+2DbuQkrWgpFtBOMuKG7A3okvWpBnu/lSNRI2\r\nV74Y9DVJWk/3LeKIRXzrTerCcX7FLwWDzSmbCPnwL+Ba8+xnrZm188ABZahDns9Y\r\nE8idikD9GtYrbvr4uKGIOcTf1L9tjTSQtm5C/uB7TppdGcD8WE+zgtM=\r\n-----END RSA PRIVATE KEY-----\r\n";

  combinedPem = replaceAll(combinedPem, "\\\\r\\\\n", "\r\n");

  String certPem = extractPemBlock(combinedPem, "-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----");
  String keyPem = extractPemBlock(combinedPem, "-----BEGIN RSA PRIVATE KEY-----", "-----END RSA PRIVATE KEY-----");

  if (certPem.length() == 0) {
    Serial.println("Certificate PEM block not found");
    return;
  }
  if (keyPem.length() == 0) {
    Serial.println("Private key PEM block not found");
    return;
  }

  Serial.println("Certificate PEM:");
  Serial.println(certPem);
  Serial.println("Private Key PEM:");
  Serial.println(keyPem);

  mbedtls_x509_crt cert;
  mbedtls_x509_crt_init(&cert);
  if (mbedtls_x509_crt_parse(&cert, (const unsigned char*)certPem.c_str(), certPem.length() + 1) != 0) {
    Serial.println("Failed to parse certificate");
    return;
  }

  mbedtls_pk_context privateKey;
  mbedtls_pk_init(&privateKey);
  if (mbedtls_pk_parse_key(&privateKey, (const unsigned char*)keyPem.c_str(), keyPem.length() + 1, nullptr, 0) != 0) {
    Serial.println("Failed to parse private key");
    return;
  }

  Serial.println("Certificate and private key parsed successfully.");

  mbedtls_x509_crt_free(&cert);
  mbedtls_pk_free(&privateKey);
}

void loop() {
  // Nothing to do here
}

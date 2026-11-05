// 🐆 Lince para Android: anuncios recompensados (Google Mobile Ads) y el
// consentimiento (UMP). Solo va en el .apk de los juegos que importan
// "anuncios"; el juego llega aquí por Servicios.llamar("anuncios.<orden>").
//
// Solo el formato recompensado y solo cuando el juego llama a mostrar: Lince
// no enseña ningún anuncio por su cuenta. Todo lo de Google va en el hilo de
// la interfaz; el juego pregunta cada fotograma (estado, resultado) y lee lo
// que quedó en estas variables.
package org.lince;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import com.google.ads.mediation.admob.AdMobAdapter;
import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.OnUserEarnedRewardListener;
import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;
import com.google.android.ump.ConsentInformation;
import com.google.android.ump.ConsentRequestParameters;
import com.google.android.ump.UserMessagingPlatform;
import org.json.JSONObject;

public class Anuncios {
    // El bloque de anuncios recompensados de prueba de Google
    private static final String BLOQUE_PRUEBAS = "ca-app-pub-3940256099942544/5224354917";

    private static String bloque;
    private static boolean pruebas = true;
    private static long espera = 10000;               // milisegundos
    private static volatile String estado = "sin_preparar";
    private static volatile String resultado = null;   // JSON, una sola vez
    private static volatile boolean mostrando = false;
    private static boolean iniciado = false, consultado = false;
    private static RewardedAd anuncio;
    private static long cargado;                       // cuándo llegó (caduca a la hora)
    private static int carga = 0;                      // para olvidar las que tardan demasiado
    private static boolean recompensado;
    private static String tipo = "";
    private static int cantidad = 0;
    private static final Handler hilo = new Handler(Looper.getMainLooper());

    public static String llamar(final Activity a, String orden, String datos) {
        switch (orden) {
            case "configurar": return configurar(datos);
            case "preparar": preparar(a); return "";
            case "estado": return estadoAhora();
            case "mostrar": return mostrar(a);
            case "resultado": {
                String r = resultado;
                resultado = null;
                return r == null ? "" : r;
            }
            case "consentimiento": return consentimiento(a);
            case "pedir_consentimiento": pedirConsentimiento(a); return "";
            default: return "error:no conozco " + orden;
        }
    }

    // "bloque\npruebas(0/1)\nespera en segundos"
    private static String configurar(String datos) {
        String[] p = datos.split("\n", -1);
        bloque = p.length > 0 ? p[0] : "";
        pruebas = p.length < 2 || !p[1].equals("0");
        try { espera = (long) (Double.parseDouble(p.length > 2 ? p[2] : "10") * 1000); } catch (NumberFormatException e) { espera = 10000; }
        return "";
    }

    private static String estadoAhora() {
        if (estado.equals("listo") && System.currentTimeMillis() - cargado > 3600 * 1000L) {
            anuncio = null;                                  // caducado: preparar() trae otro
            estado = "error";
        }
        return estado;
    }

    private static boolean hayConexion(Activity a) {
        try {
            android.net.ConnectivityManager cm = (android.net.ConnectivityManager) a.getSystemService(Activity.CONNECTIVITY_SERVICE);
            android.net.Network n = cm == null ? null : cm.getActiveNetwork();
            return n != null;
        } catch (Exception e) {
            return true;                                    // si no se sabe, se intenta
        }
    }

    private static void preparar(final Activity a) {
        if (mostrando || estado.equals("cargando") || estado.equals("listo")) return;
        if (!hayConexion(a)) { estado = "sin_conexion"; return; }
        estado = "cargando";
        final int esta = ++carga;
        hilo.post(new Runnable() {
            @Override public void run() {
                final ConsentInformation ci = UserMessagingPlatform.getConsentInformation(a);
                if (!consultado) {                           // pregunta si hace falta consentimiento (sin formulario)
                    consultado = true;
                    ci.requestConsentInfoUpdate(a, new ConsentRequestParameters.Builder().build(),
                        new ConsentInformation.OnConsentInfoUpdateSuccessListener() {
                            @Override public void onConsentInfoUpdateSuccess() { cargar(a, esta); }
                        },
                        new ConsentInformation.OnConsentInfoUpdateFailureListener() {
                            @Override public void onConsentInfoUpdateFailure(com.google.android.ump.FormError e) { cargar(a, esta); }
                        });
                } else cargar(a, esta);
            }
        });
        hilo.postDelayed(new Runnable() {                   // nunca una espera infinita
            @Override public void run() {
                if (carga == esta && estado.equals("cargando")) { carga++; estado = hayConexion(a) ? "error" : "sin_conexion"; }
            }
        }, espera);
    }

    private static void cargar(final Activity a, final int esta) {
        ConsentInformation ci = UserMessagingPlatform.getConsentInformation(a);
        if (!ci.canRequestAds()) { if (carga == esta) estado = "falta_consentimiento"; return; }
        if (!iniciado) {
            iniciado = true;
            MobileAds.initialize(a);
        }
        AdRequest.Builder peticion = new AdRequest.Builder();
        if (consentimiento(a).equals("denegado")) {          // sin consentimiento, nada personalizado
            Bundle extras = new Bundle();
            extras.putString("npa", "1");
            peticion.addNetworkExtrasBundle(AdMobAdapter.class, extras);
        }
        String b = pruebas || bloque == null || bloque.isEmpty() ? BLOQUE_PRUEBAS : bloque;
        RewardedAd.load(a, b, peticion.build(), new RewardedAdLoadCallback() {
            @Override public void onAdLoaded(RewardedAd ad) {
                if (carga != esta) return;                   // llegó tarde: ya se dio por fallido
                anuncio = ad;
                cargado = System.currentTimeMillis();
                estado = "listo";
            }
            @Override public void onAdFailedToLoad(LoadAdError e) {
                android.util.Log.w("lince", "anuncios: no se pudo cargar: " + e);    // con 'adb logcat -s lince'
                if (carga != esta) return;
                anuncio = null;
                estado = e.getCode() == AdRequest.ERROR_CODE_NETWORK_ERROR ? "sin_conexion" : "error";
            }
        });
    }

    private static String mostrar(final Activity a) {
        if (mostrando) return "error:ya se está mostrando un anuncio";
        if (!estadoAhora().equals("listo") || anuncio == null) return "error:no hay ningún anuncio listo (estado() es \"" + estado + "\"): antes, preparar()";
        mostrando = true;
        recompensado = false;
        final RewardedAd ad = anuncio;
        anuncio = null;
        estado = "sin_preparar";
        hilo.post(new Runnable() {
            @Override public void run() {
                ad.setFullScreenContentCallback(new FullScreenContentCallback() {
                    @Override public void onAdDismissedFullScreenContent() {
                        terminar(recompensado ? "completado" : "cerrado");
                    }
                    @Override public void onAdFailedToShowFullScreenContent(AdError e) {
                        terminar(hayConexion(a) ? "error" : "sin_conexion");
                    }
                });
                ad.show(a, new OnUserEarnedRewardListener() {
                    @Override public void onUserEarnedReward(RewardItem r) {
                        recompensado = true;                 // solo cuando Google lo confirma
                        tipo = r.getType();
                        cantidad = r.getAmount();
                    }
                });
            }
        });
        return "";
    }

    private static void terminar(String motivo) {
        try {
            JSONObject r = new JSONObject();
            r.put("recompensa", recompensado);
            r.put("motivo", motivo);
            r.put("tipo", recompensado ? tipo : "");
            r.put("cantidad", recompensado ? cantidad : 0);
            resultado = r.toString();
        } catch (Exception e) {
            resultado = "{\"recompensa\": false, \"motivo\": \"error\", \"tipo\": \"\", \"cantidad\": 0}";
        }
        mostrando = false;
    }

    // Lo que dijo el jugador, por el estándar de IAB (TCF) que guarda UMP: el
    // propósito 1 (guardar y leer en el aparato) es el que necesitan los anuncios
    private static String consentimiento(Activity a) {
        ConsentInformation ci = UserMessagingPlatform.getConsentInformation(a);
        int s = ci.getConsentStatus();
        if (s == ConsentInformation.ConsentStatus.NOT_REQUIRED) return "no_hace_falta";
        if (s != ConsentInformation.ConsentStatus.OBTAINED) return "pendiente";
        SharedPreferences p = PreferenceManager.getDefaultSharedPreferences(a);
        String propositos = p.getString("IABTCF_PurposeConsents", "");
        return propositos.length() > 0 && propositos.charAt(0) == '1' ? "concedido" : "denegado";
    }

    private static void pedirConsentimiento(final Activity a) {
        hilo.post(new Runnable() {
            @Override public void run() {
                final ConsentInformation ci = UserMessagingPlatform.getConsentInformation(a);
                ci.requestConsentInfoUpdate(a, new ConsentRequestParameters.Builder().build(),
                    new ConsentInformation.OnConsentInfoUpdateSuccessListener() {
                        @Override public void onConsentInfoUpdateSuccess() {
                            consultado = true;
                            UserMessagingPlatform.loadAndShowConsentFormIfRequired(a,
                                new com.google.android.ump.ConsentForm.OnConsentFormDismissedListener() {
                                    @Override public void onConsentFormDismissed(com.google.android.ump.FormError e) {
                                        if (estado.equals("falta_consentimiento")) estado = "sin_preparar";
                                    }
                                });
                        }
                    },
                    new ConsentInformation.OnConsentInfoUpdateFailureListener() {
                        @Override public void onConsentInfoUpdateFailure(com.google.android.ump.FormError e) { }
                    });
            }
        });
    }
}

// 🐆 Lince para Android: la puerta entre el juego (en C) y lo que solo se
// puede pedir en Java. src/android.c llama a llamar(actividad, orden, datos)
// con los textos en UTF-8 y recibe la respuesta igual.
//
//   "conexion"         → "con_conexion", "sin_conexion" o "desconocida"
//   "conexion_medida"  → "si", "no" o "desconocida"
//   "anuncios.…"       → org.lince.Anuncios, si el .apk lo lleva
//   "compras.…"        → org.lince.Compras, si el .apk lo lleva
//
// Anuncios y Compras solo van en el .apk de los juegos que importan esos
// módulos (con su biblioteca de Google): si no están, "no_disponible".
package org.lince;

import android.app.Activity;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;

public class Servicios {
    public static byte[] llamar(Activity actividad, byte[] orden, byte[] datos) {
        String o = new String(orden, StandardCharsets.UTF_8);
        String d = new String(datos, StandardCharsets.UTF_8);
        String r;
        try {
            if (o.equals("conexion")) r = conexion(actividad);
            else if (o.equals("conexion_medida")) r = medida(actividad);
            else if (o.startsWith("anuncios.")) r = otra("org.lince.Anuncios", actividad, o.substring(9), d);
            else if (o.startsWith("compras.")) r = otra("org.lince.Compras", actividad, o.substring(8), d);
            else r = "desconocida";
        } catch (Throwable t) {
            r = "error:" + t;
        }
        return r.getBytes(StandardCharsets.UTF_8);
    }

    // Anuncios y Compras, si están en el .apk (por su nombre: si no, ni se cargan)
    private static String otra(String clase, Activity a, String orden, String datos) throws Exception {
        Class<?> c;
        try {
            c = Class.forName(clase);
        } catch (ClassNotFoundException e) {
            return "no_disponible";
        }
        Method m = c.getMethod("llamar", Activity.class, String.class, String.class);
        Object r = m.invoke(null, a, orden, datos);
        return r == null ? "" : r.toString();
    }

    // La red activa, con Internet comprobado (VALIDATED: no un wifi con portal
    // cautivo). Sin el permiso ACCESS_NETWORK_STATE, "desconocida".
    private static String conexion(Activity a) {
        try {
            ConnectivityManager cm = (ConnectivityManager) a.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null) return "desconocida";
            Network n = cm.getActiveNetwork();
            if (n == null) return "sin_conexion";
            NetworkCapabilities c = cm.getNetworkCapabilities(n);
            if (c == null) return "sin_conexion";
            return c.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
                   c.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED) ? "con_conexion" : "sin_conexion";
        } catch (SecurityException e) {
            return "desconocida";
        }
    }

    private static String medida(Activity a) {
        try {
            ConnectivityManager cm = (ConnectivityManager) a.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null || cm.getActiveNetwork() == null) return "desconocida";
            return cm.isActiveNetworkMetered() ? "si" : "no";
        } catch (SecurityException e) {
            return "desconocida";
        }
    }
}

// 🐆 Lince para Android: compras de productos únicos (no consumibles) con
// Google Play Billing. Solo va en el .apk de los juegos que importan
// "compras"; el juego llega aquí por Servicios.llamar("compras.<orden>").
//
// Una compra no se da por buena hasta que Google confirma el
// acknowledgePurchase (si no, la devuelve a los tres días). Lince no guarda
// nada: lo que se tiene se pregunta a Google Play (restaurar). Lo de Google
// va en sus hilos; el juego pregunta cada fotograma y lee lo que quedó aquí.
package org.lince;

import android.app.Activity;
import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.AcknowledgePurchaseResponseListener;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.ProductDetailsResponseListener;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesResponseListener;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryProductDetailsResult;
import com.android.billingclient.api.QueryPurchasesParams;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONObject;

public class Compras {
    private static BillingClient cliente;
    private static volatile String estado = "conectando";
    private static volatile String productos = null;    // JSON, cuando llegan
    private static volatile String resultado = null;    // JSON, una sola vez
    private static volatile String comprados = null;    // JSON, cuando llegan
    private static volatile boolean comprando = false;
    private static String pedido = "";                  // el id de la compra en curso
    private static final Map<String, ProductDetails> detalles = new HashMap<>();

    public static String llamar(final Activity a, String orden, String datos) {
        conectar(a);
        switch (orden) {
            case "estado": return estado;
            case "consultar": consultar(datos); return "";
            case "productos": { String p = productos; return p == null ? "" : p; }
            case "comprar": return comprar(a, datos);
            case "resultado_compra": { String r = resultado; resultado = null; return r == null ? "" : r; }
            case "restaurar": restaurar(); return "";
            case "comprados": { String c = comprados; return c == null ? "" : c; }
            default: return "error:no conozco " + orden;
        }
    }

    private static synchronized void conectar(Activity a) {
        if (cliente != null) return;
        cliente = BillingClient.newBuilder(a.getApplicationContext())
            .setListener(new PurchasesUpdatedListener() {
                @Override public void onPurchasesUpdated(BillingResult r, List<Purchase> compras) { comprado(r, compras); }
            })
            .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
            .enableAutoServiceReconnection()                 // se reconecta solo si se cae
            .build();
        cliente.startConnection(new BillingClientStateListener() {
            @Override public void onBillingSetupFinished(BillingResult r) { estado = estadoDe(r.getResponseCode()); }
            @Override public void onBillingServiceDisconnected() { estado = "conectando"; }
        });
    }

    private static String estadoDe(int codigo) {
        switch (codigo) {
            case BillingClient.BillingResponseCode.OK: return "lista";
            case BillingClient.BillingResponseCode.SERVICE_UNAVAILABLE:
            case BillingClient.BillingResponseCode.NETWORK_ERROR: return "sin_conexion";
            case BillingClient.BillingResponseCode.SERVICE_DISCONNECTED: return "conectando";
            default: return "no_disponible";                 // sin Google Play, o una que no sirve
        }
    }

    private static String motivoDe(int codigo) {
        switch (codigo) {
            case BillingClient.BillingResponseCode.USER_CANCELED: return "cancelado";
            case BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED: return "ya_comprado";
            case BillingClient.BillingResponseCode.SERVICE_UNAVAILABLE:
            case BillingClient.BillingResponseCode.NETWORK_ERROR:
            case BillingClient.BillingResponseCode.SERVICE_DISCONNECTED: return "sin_conexion";
            case BillingClient.BillingResponseCode.BILLING_UNAVAILABLE:
            case BillingClient.BillingResponseCode.FEATURE_NOT_SUPPORTED: return "no_disponible";
            default: return "error";
        }
    }

    // datos: los ids, uno por línea
    private static void consultar(String datos) {
        productos = null;
        List<QueryProductDetailsParams.Product> lista = new ArrayList<>();
        for (String id : datos.split("\n"))
            if (!id.isEmpty())
                lista.add(QueryProductDetailsParams.Product.newBuilder().setProductId(id)
                    .setProductType(BillingClient.ProductType.INAPP).build());
        if (lista.isEmpty() || estado.equals("no_disponible") || estado.equals("sin_conexion")) { productos = "[]"; return; }
        cliente.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(lista).build(),
            new ProductDetailsResponseListener() {
                @Override public void onProductDetailsResponse(BillingResult r, QueryProductDetailsResult res) {
                    JSONArray l = new JSONArray();
                    try {
                        if (r.getResponseCode() == BillingClient.BillingResponseCode.OK)
                            for (ProductDetails p : res.getProductDetailsList()) {
                                synchronized (detalles) { detalles.put(p.getProductId(), p); }
                                JSONObject o = new JSONObject();
                                o.put("id", p.getProductId());
                                o.put("titulo", p.getName());
                                o.put("descripcion", p.getDescription());
                                ProductDetails.OneTimePurchaseOfferDetails oferta = p.getOneTimePurchaseOfferDetails();
                                o.put("precio", oferta == null ? "" : oferta.getFormattedPrice());
                                l.put(o);
                            }
                    } catch (Exception e) { /* lo que se pudo leer */ }
                    productos = l.toString();
                }
            });
    }

    private static String comprar(Activity a, String id) {
        if (comprando) return "error:ya hay una compra en curso";
        if (!estado.equals("lista")) {                       // sin Google Play o sin conexión: se dice, no es un error
            comprando = true;
            terminar(id, estado.equals("sin_conexion") ? "sin_conexion" : "no_disponible", null);
            return "";
        }
        ProductDetails p;
        synchronized (detalles) { p = detalles.get(id); }
        if (p == null) return "error:'" + id + "' no está entre los productos (consultar_productos() antes)";
        comprando = true;
        pedido = id;
        List<BillingFlowParams.ProductDetailsParams> l = new ArrayList<>();
        l.add(BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(p).build());
        BillingResult r = cliente.launchBillingFlow(a, BillingFlowParams.newBuilder().setProductDetailsParamsList(l).build());
        if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) terminar(id, motivoDe(r.getResponseCode()), r.getDebugMessage());
        return "";
    }

    private static void terminar(String id, String motivo, String mensaje) {
        try {
            JSONObject o = new JSONObject();
            o.put("id", id);
            o.put("resultado", motivo);
            o.put("mensaje", mensaje == null || mensaje.isEmpty() ? JSONObject.NULL : mensaje);
            resultado = o.toString();
        } catch (Exception e) {
            resultado = "{\"id\": \"\", \"resultado\": \"error\", \"mensaje\": null}";
        }
        comprando = false;
    }

    private static void comprado(BillingResult r, List<Purchase> compras) {
        if (r.getResponseCode() != BillingClient.BillingResponseCode.OK || compras == null) {
            if (comprando) terminar(pedido, motivoDe(r.getResponseCode()), r.getDebugMessage());
            return;
        }
        for (final Purchase c : compras) {
            final String id = c.getProducts().isEmpty() ? pedido : c.getProducts().get(0);
            if (c.getPurchaseState() == Purchase.PurchaseState.PENDING) terminar(id, "pendiente", null);
            else if (c.getPurchaseState() == Purchase.PurchaseState.PURCHASED) confirmar(c, id, true);
        }
    }

    // acknowledge: hasta que Google lo confirma no es "comprado"
    private static void confirmar(Purchase c, final String id, final boolean avisar) {
        if (c.isAcknowledged()) { if (avisar) terminar(id, "comprado", null); return; }
        cliente.acknowledgePurchase(AcknowledgePurchaseParams.newBuilder().setPurchaseToken(c.getPurchaseToken()).build(),
            new AcknowledgePurchaseResponseListener() {
                @Override public void onAcknowledgePurchaseResponse(BillingResult r) {
                    boolean bien = r.getResponseCode() == BillingClient.BillingResponseCode.OK;
                    if (avisar || bien) terminar(id, bien ? "comprado" : motivoDe(r.getResponseCode()), bien ? null : r.getDebugMessage());
                }
            });
    }

    // Lo que tiene el jugador ahora: solo las compras en vigor (no las devueltas).
    // Una que estaba pendiente y ya se pagó se confirma aquí, y lo dice resultado_compra.
    private static void restaurar() {
        comprados = null;
        if (estado.equals("no_disponible") || estado.equals("sin_conexion")) {   // no esperar a una respuesta que no llega
            comprados = "{\"ok\": false, \"productos\": []}";
            return;
        }
        cliente.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
            new PurchasesResponseListener() {
                @Override public void onQueryPurchasesResponse(BillingResult r, List<Purchase> compras) {
                    JSONArray ids = new JSONArray();
                    boolean bien = r.getResponseCode() == BillingClient.BillingResponseCode.OK;
                    if (bien && compras != null)
                        for (Purchase c : compras)
                            if (c.getPurchaseState() == Purchase.PurchaseState.PURCHASED) {
                                for (String id : c.getProducts()) ids.put(id);
                                if (!c.isAcknowledged()) confirmar(c, c.getProducts().isEmpty() ? "" : c.getProducts().get(0), false);
                            }
                    try {
                        JSONObject o = new JSONObject();
                        o.put("ok", bien);
                        o.put("productos", ids);
                        comprados = o.toString();
                    } catch (Exception e) {
                        comprados = "{\"ok\": false, \"productos\": []}";
                    }
                }
            });
    }
}

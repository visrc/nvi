typedef struct {
    GtkViScreen  *vi;
    GtkWidget	*main;
    GtkWidget	*term;
    gint    input_func;
    gint    value_changed;
    IPVI    *ipvi;
    int	    resized;
} gtk_vi;

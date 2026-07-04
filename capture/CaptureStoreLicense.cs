using System.Diagnostics;
using Windows.Services.Store;

internal static partial class Program
{
    private static async Task<int> CheckStoreLicenseAsync()
    {
        try
        {
            StoreContext context = StoreContext.GetDefault();
            StoreAppLicense license = await context.GetAppLicenseAsync();
            return license.IsActive ? 0 : 2;
        }
        catch (Exception ex)
        {
            Debug.WriteLine(ex);
            return 1;
        }
    }

}

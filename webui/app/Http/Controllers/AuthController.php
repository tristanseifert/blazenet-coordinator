<?php

namespace App\Http\Controllers;

use App\Models\User;
use Carbon\Carbon;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Auth;
use Illuminate\Support\Facades\Hash;
use Illuminate\Support\Facades\Log;
use Illuminate\Validation\Rules\Password;

/**
 * @brief Authentication (login, password reset, etc.)
 */
class AuthController extends Controller
{
    /**
     * Registers the routes.
     */
    public static function RegisterRoutes(\Illuminate\Routing\Router $router) {
        $router->get('/login', [static::class, 'login'])->name('login')->middleware('guest');
        $router->post('/login', [static::class, 'doLogin'])->middleware('guest');

        $router->get('/changePassword', [static::class, 'changePassword'])
               ->name('auth.changePassword')->middleware('auth');
        $router->post('/changePassword', [static::class, 'doChangePassword'])->middleware('auth');

        $router->post('/logout', [static::class, 'logout'])->name('logout')->middleware('auth');
    }

    /**
     * Render the login form.
     */
    public function login(Request $req) {
        return view('auth.login');
    }

    /**
     * Handle the login. We'll first look up the user by username, then invoke the appropriate
     * authentication strategy.
     */
    public function doLogin(Request $req) {
        // validate request and fetch the user record
        $credentials = $req->validate([
            'username'      => 'required|string',
            'password'      => 'nullable|string',
            'remember'      => 'nullable|boolean',
        ]);

        $user = User::where('username', $req->get('username'))->first();
        if(!$user) {
            return redirect()->back()->withInput()->withErrors([
                'message' => __('auth.login.invalidcreds')
            ]);
        }

        // use built-in authentication
        if(Auth::attempt([
            'username'      => $credentials['username'],
            'password'      => $credentials['password'],
            'is_enabled'    => true,
        ], isset($credentials['remember']))) {
            // refresh session id and get user info
            $req->session()->regenerate();
            $user = auth()->user();

            // force the password to be updated otherwise
            if($user->password_expiration && $user->password_expiration->isPast()) {
                flash()->info(__('auth.login.passwordExpired'));
                return redirect()->route('auth.changePassword');
            }

            // proceed with the login as normal
            flash()->success(__('auth.login.success'));

            $user->last_login = Carbon::now();
            $user->save();

            return redirect()->intended(route('homepage'));
        }

        // authentication failed
        return redirect()->back()->withInput()->withErrors([
            'message' => __('auth.login.invalidcreds')
        ]);
    }

    /**
     * Log out the current user.
     */
    public function logout(Request $req) {
        // destroy session
        Auth::logout();
        $req->session()->regenerate();

        // redirect to homepage
        flash()->success(__('auth.logout.success'));
        return redirect(route('login'));
    }

    /**
     * @brief Render the "change password" form.
     *
     * This is used for the case when a password expires. Therefore, this renders a view that
     * requires the user's current password to be entered.
     */
    public function changePassword(Request $req) {
        return view('auth.changePassword');
    }

    /**
     * @brief Process the change password process.
     *
     * Otherwise, we'll verify the form data, attempt to re-authenticate the user (with their
     * provided password) and then change the password. Afterwards, we'll log out again and force
     * the user to log in with the new, unexpired password.
     */
    public function doChangePassword(Request $req) {
        // validate the request
        $credentials = $req->validate([
            'password'      => 'required|string|current_password',
            'newPassword'   => ['required', 'string', 'confirmed', Password::defaults()]
        ]);
        // TODO: verify the new password is different than current password

        // current password valid, and new password valid, so update it
        $user = auth()->user();

        $user->password = Hash::make($credentials['newPassword']);
        $user->password_expiration = null;
        $user->password_changed_on = Carbon::now();

        $user->save();

        // log the user out so they can try again
        Auth::logout();
        $req->session()->regenerate();
        flash()->success(__('auth.changePassword.success'));
        return redirect(route('login'));
    }
}

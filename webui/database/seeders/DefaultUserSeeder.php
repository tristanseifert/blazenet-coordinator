<?php

namespace Database\Seeders;

use Carbon\Carbon;
use Illuminate\Database\Console\Seeds\WithoutModelEvents;
use Illuminate\Database\Seeder;
use Illuminate\Support\Facades\DB;
use Illuminate\Support\Facades\Hash;

/**
 * @brief Create the default administrative user.
 */
class DefaultUserSeeder extends Seeder
{
    /**
     * Run the database seeds.
     *
     * @return void
     */
    public function run()
    {
        DB::table('users')->insert([
            'username'              => 'admin',
            'display_name'          => 'Administrator',
            'password'              => Hash::make('plschangeme69'),
            // the password _must_ be changed on login
            'password_expiration'   => Carbon::create(1970, 4, 20, 16, 20),
            'password_changed_on'   => Carbon::now(),
        ]);
    }
}

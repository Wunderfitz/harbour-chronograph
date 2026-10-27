/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Chronograph.

    Chronograph is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Chronograph is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Chronograph. If not, see <http://www.gnu.org/licenses/>.
*/

// The occasions the calendar achievements collect, by their keys in
// AchievementCatalogue.cpp: what happened, or what is celebrated, on each.
// The only place they are named. The year comes from the catalogue, and
// title() puts it after the name. A new occasion gets its name here.

function title(key, year) {
    if (year > 0)
        //: An occasion of a calendar achievement and the year it happened
        return qsTr("%1 (%2)").arg(name(key)).arg(year)
    if (year < 0)
        //: An occasion of a calendar achievement and the year before Christ it happened
        return qsTr("%1 (%2 BC)").arg(name(key)).arg(-year)
    return name(key)
}

function name(key) {
    switch (key) {
    // patriot
    case "australia-day":
        return qsTr("Australia: Australia Day")
    case "new-zealand-waitangi-day":
        return qsTr("New Zealand: Waitangi Day")
    case "japan-foundation-day":
        return qsTr("Japan: National Foundation Day")
    case "estonia-independence-day":
        return qsTr("Estonia: Independence Day")
    case "korea-independence-movement-day":
        return qsTr("South Korea: Independence Movement Day")
    case "ghana-independence-day":
        return qsTr("Ghana: Independence Day")
    case "ireland-st-patricks-day":
        return qsTr("Ireland: Saint Patrick's Day")
    case "greece-independence-day":
        return qsTr("Greece: Independence Day")
    case "south-africa-freedom-day":
        return qsTr("South Africa: Freedom Day")
    case "norway-constitution-day":
        return qsTr("Norway: Constitution Day")
    case "argentina-may-revolution":
        return qsTr("Argentina: May Revolution Day")
    case "italy-republic-day":
        return qsTr("Italy: Republic Day")
    case "denmark-constitution-day":
        return qsTr("Denmark: Constitution Day")
    case "sweden-national-day":
        return qsTr("Sweden: National Day")
    case "portugal-day":
        return qsTr("Portugal: Portugal Day")
    case "russia-day":
        return qsTr("Russia: Russia Day")
    case "canada-day":
        return qsTr("Canada: Canada Day")
    case "united-states-independence-day":
        return qsTr("United States: Independence Day")
    case "france-bastille-day":
        return qsTr("France: Bastille Day")
    case "colombia-independence-day":
        return qsTr("Colombia: Independence Day")
    case "belgium-national-day":
        return qsTr("Belgium: National Day")
    case "egypt-revolution-day":
        return qsTr("Egypt: Revolution Day")
    case "peru-independence-day":
        return qsTr("Peru: Independence Day")
    case "switzerland-national-day":
        return qsTr("Switzerland: National Day")
    case "pakistan-independence-day":
        return qsTr("Pakistan: Independence Day")
    case "india-independence-day":
        return qsTr("India: Independence Day")
    case "indonesia-independence-day":
        return qsTr("Indonesia: Independence Day")
    case "hungary-st-stephens-day":
        return qsTr("Hungary: Saint Stephen's Day")
    case "ukraine-independence-day":
        return qsTr("Ukraine: Independence Day")
    case "vietnam-national-day":
        return qsTr("Vietnam: National Day")
    case "brazil-independence-day":
        return qsTr("Brazil: Independence Day")
    case "mexico-independence-day":
        return qsTr("Mexico: Independence Day")
    case "chile-independence-day":
        return qsTr("Chile: Independence Day")
    case "china-national-day":
        return qsTr("China: National Day")
    case "germany-unity-day":
        return qsTr("Germany: Day of German Unity")
    case "spain-national-day":
        return qsTr("Spain: National Day")
    case "austria-national-day":
        return qsTr("Austria: National Day")
    case "czechia-independence-day":
        return qsTr("Czechia: Independence Day")
    case "turkey-republic-day":
        return qsTr("Turkey: Republic Day")
    case "poland-independence-day":
        return qsTr("Poland: Independence Day")
    case "finland-independence-day":
        return qsTr("Finland: Independence Day")
    case "kenya-jamhuri-day":
        return qsTr("Kenya: Jamhuri Day")

    // worshipper
    case "epiphany":
        return qsTr("Christianity: Epiphany")
    case "orthodox-christmas":
        return qsTr("Christianity: Orthodox Christmas")
    case "makar-sankranti":
        return qsTr("Hinduism: Makar Sankranti")
    case "nirvana-day":
        return qsTr("Buddhism: Nirvana Day")
    case "ramadan":
        return qsTr("Islam: First day of Ramadan")
    case "maha-shivaratri":
        return qsTr("Hinduism: Maha Shivaratri")
    case "ash-wednesday":
        return qsTr("Christianity: Ash Wednesday")
    case "purim":
        return qsTr("Judaism: Purim")
    case "holi":
        return qsTr("Hinduism: Holi")
    case "laylat-al-qadr":
        return qsTr("Islam: Night of Power")
    case "eid-al-fitr":
        return qsTr("Islam: Eid al-Fitr")
    case "nowruz":
        return qsTr("Zoroastrianism and Bahá'í: Nowruz")
    case "passover":
        return qsTr("Judaism: Passover")
    case "good-friday":
        return qsTr("Christianity: Good Friday")
    case "easter":
        return qsTr("Christianity: Easter Sunday")
    case "orthodox-easter":
        return qsTr("Christianity: Orthodox Easter")
    case "vaisakhi":
        return qsTr("Sikhism: Vaisakhi")
    case "vesak":
        return qsTr("Buddhism: Vesak")
    case "ascension":
        return qsTr("Christianity: Ascension Day")
    case "shavuot":
        return qsTr("Judaism: Shavuot")
    case "pentecost":
        return qsTr("Christianity: Pentecost")
    case "day-of-arafah":
        return qsTr("Islam: Day of Arafah")
    case "eid-al-adha":
        return qsTr("Islam: Eid al-Adha")
    case "corpus-christi":
        return qsTr("Christianity: Corpus Christi")
    case "islamic-new-year":
        return qsTr("Islam: Islamic New Year")
    case "ashura":
        return qsTr("Islam: Ashura")
    case "mawlid":
        return qsTr("Islam: The Prophet's Birthday")
    case "assumption":
        return qsTr("Christianity: Assumption of Mary")
    case "janmashtami":
        return qsTr("Hinduism: Krishna Janmashtami")
    case "ganesh-chaturthi":
        return qsTr("Hinduism: Ganesh Chaturthi")
    case "rosh-hashanah":
        return qsTr("Judaism: Rosh Hashanah")
    case "yom-kippur":
        return qsTr("Judaism: Yom Kippur")
    case "sukkot":
        return qsTr("Judaism: Sukkot")
    case "navaratri":
        return qsTr("Hinduism: First day of Navaratri")
    case "dussehra":
        return qsTr("Hinduism: Dussehra")
    case "reformation-day":
        return qsTr("Christianity: Reformation Day")
    case "all-saints":
        return qsTr("Christianity: All Saints' Day")
    case "diwali":
        return qsTr("Hinduism: Diwali")
    case "guru-nanak-gurpurab":
        return qsTr("Sikhism: Guru Nanak Gurpurab")
    case "hanukkah":
        return qsTr("Judaism: First day of Hanukkah")
    case "bodhi-day":
        return qsTr("Buddhism: Bodhi Day")
    case "christmas":
        return qsTr("Christianity: Christmas Day")

    // freedom-fighter
    case "death-galtieri":
        return qsTr("Leopoldo Galtieri dies")
    case "death-lenin":
        return qsTr("Vladimir Lenin dies")
    case "death-suharto":
        return qsTr("Suharto dies")
    case "death-metaxas":
        return qsTr("Ioannis Metaxas dies")
    case "death-mubarak":
        return qsTr("Hosni Mubarak dies")
    case "death-stalin":
        return qsTr("Joseph Stalin dies")
    case "death-caesar":
        return qsTr("Julius Caesar is murdered")
    case "death-primo-de-rivera":
        return qsTr("Miguel Primo de Rivera dies")
    case "death-chiang-kai-shek":
        return qsTr("Chiang Kai-shek dies")
    case "death-hoxha":
        return qsTr("Enver Hoxha dies")
    case "death-pol-pot":
        return qsTr("Pol Pot dies")
    case "death-francois-duvalier":
        return qsTr("François Duvalier dies")
    case "death-mussolini":
        return qsTr("Benito Mussolini is executed")
    case "death-hitler":
        return qsTr("Adolf Hitler dies")
    case "death-tito":
        return qsTr("Josip Broz Tito dies")
    case "death-videla":
        return qsTr("Jorge Rafael Videla dies")
    case "death-honecker":
        return qsTr("Erich Honecker dies")
    case "death-trujillo":
        return qsTr("Rafael Trujillo is murdered")
    case "death-antonescu":
        return qsTr("Ion Antonescu is executed")
    case "death-abacha":
        return qsTr("Sani Abacha dies")
    case "death-hafez-al-assad":
        return qsTr("Hafez al-Assad dies")
    case "death-kim-il-sung":
        return qsTr("Kim Il-sung dies")
    case "death-salazar":
        return qsTr("António de Oliveira Salazar dies")
    case "death-batista":
        return qsTr("Fulgencio Batista dies")
    case "death-idi-amin":
        return qsTr("Idi Amin dies")
    case "death-mugabe":
        return qsTr("Robert Mugabe dies")
    case "death-mobutu":
        return qsTr("Mobutu Sese Seko dies")
    case "death-mao":
        return qsTr("Mao Zedong dies")
    case "death-somoza":
        return qsTr("Anastasio Somoza Debayle is killed")
    case "death-marcos":
        return qsTr("Ferdinand Marcos dies")
    case "death-gaddafi":
        return qsTr("Muammar al-Gaddafi is killed")
    case "death-park-chung-hee":
        return qsTr("Park Chung-hee is murdered")
    case "death-bokassa":
        return qsTr("Jean-Bédel Bokassa dies")
    case "death-franco":
        return qsTr("Francisco Franco dies")
    case "death-castro":
        return qsTr("Fidel Castro dies")
    case "death-ne-win":
        return qsTr("Ne Win dies")
    case "death-pinochet":
        return qsTr("Augusto Pinochet dies")
    case "death-kim-jong-il":
        return qsTr("Kim Jong-il dies")
    case "death-niyazov":
        return qsTr("Saparmurat Niyazov dies")
    case "death-ceausescu":
        return qsTr("Nicolae Ceaușescu is executed")
    case "death-pavelic":
        return qsTr("Ante Pavelić dies")
    case "death-saddam-hussein":
        return qsTr("Saddam Hussein is executed")

    // warmonger
    case "russo-japanese-war":
        return qsTr("The Russo-Japanese War begins")
    case "invasion-of-ukraine":
        return qsTr("Russia invades Ukraine")
    case "syrian-civil-war":
        return qsTr("The Syrian Civil War begins")
    case "iraq-war":
        return qsTr("The Iraq War begins")
    case "kosovo-war":
        return qsTr("NATO's air war over Kosovo begins")
    case "falklands-war":
        return qsTr("The Falklands War begins")
    case "war-of-the-pacific":
        return qsTr("The War of the Pacific begins")
    case "bosnian-war":
        return qsTr("The Bosnian War begins")
    case "american-civil-war":
        return qsTr("The American Civil War begins")
    case "american-revolutionary-war":
        return qsTr("The American Revolutionary War begins")
    case "spanish-american-war":
        return qsTr("The Spanish-American War begins")
    case "mexican-american-war":
        return qsTr("The Mexican-American War begins")
    case "war-of-the-spanish-succession":
        return qsTr("The War of the Spanish Succession is declared")
    case "napoleonic-wars":
        return qsTr("The Napoleonic Wars begin")
    case "thirty-years-war":
        return qsTr("The Thirty Years' War begins")
    case "six-day-war":
        return qsTr("The Six-Day War begins")
    case "austro-prussian-war":
        return qsTr("The Austro-Prussian War begins")
    case "war-of-1812":
        return qsTr("The War of 1812 begins")
    case "operation-barbarossa":
        return qsTr("Germany invades the Soviet Union")
    case "korean-war":
        return qsTr("The Korean War begins")
    case "nigerian-civil-war":
        return qsTr("The Nigerian Civil War begins")
    case "second-sino-japanese-war":
        return qsTr("The Second Sino-Japanese War begins")
    case "spanish-civil-war":
        return qsTr("The Spanish Civil War begins")
    case "franco-prussian-war":
        return qsTr("The Franco-Prussian War begins")
    case "first-world-war":
        return qsTr("The First World War begins")
    case "chinese-civil-war":
        return qsTr("The Chinese Civil War begins")
    case "gulf-war":
        return qsTr("Iraq invades Kuwait: the Gulf War begins")
    case "seven-years-war":
        return qsTr("The Seven Years' War begins")
    case "second-world-war":
        return qsTr("The Second World War begins")
    case "first-opium-war":
        return qsTr("The First Opium War begins")
    case "iran-iraq-war":
        return qsTr("The Iran-Iraq War begins")
    case "yom-kippur-war":
        return qsTr("The Yom Kippur War begins")
    case "war-in-afghanistan":
        return qsTr("The War in Afghanistan begins")
    case "first-balkan-war":
        return qsTr("The First Balkan War begins")
    case "second-boer-war":
        return qsTr("The Second Boer War begins")
    case "vietnam-war":
        return qsTr("The Vietnam War begins")
    case "winter-war":
        return qsTr("The Winter War begins")
    case "indo-pakistani-war":
        return qsTr("The Indo-Pakistani War of 1971 begins")
    case "pacific-war":
        return qsTr("Japan attacks Pearl Harbor")
    case "war-of-the-austrian-succession":
        return qsTr("The War of the Austrian Succession begins")
    case "first-indochina-war":
        return qsTr("The First Indochina War begins")
    case "soviet-afghan-war":
        return qsTr("The Soviet Union invades Afghanistan")

    // peacemaker
    case "treaty-of-kiel":
        return qsTr("Treaty of Kiel")
    case "elysee-treaty":
        return qsTr("Élysée Treaty of Franco-German friendship")
    case "treaty-of-karlowitz":
        return qsTr("Treaty of Karlowitz")
    case "paris-peace-accords":
        return qsTr("Paris Peace Accords on Vietnam")
    case "treaty-of-guadalupe-hidalgo":
        return qsTr("Treaty of Guadalupe Hidalgo")
    case "treaty-of-paris-1763":
        return qsTr("Treaty of Paris ends the Seven Years' War")
    case "treaty-of-brest-litovsk":
        return qsTr("Treaty of Brest-Litovsk")
    case "moscow-peace-treaty":
        return qsTr("Moscow Peace Treaty ends the Winter War")
    case "treaty-of-paris-1856":
        return qsTr("Treaty of Paris ends the Crimean War")
    case "end-of-spanish-civil-war":
        return qsTr("The Spanish Civil War ends")
    case "appomattox":
        return qsTr("Surrender at Appomattox ends the American Civil War")
    case "good-friday-agreement":
        return qsTr("Good Friday Agreement for Northern Ireland")
    case "treaty-of-utrecht":
        return qsTr("Peace of Utrecht")
    case "ve-day":
        return qsTr("The Second World War ends in Europe")
    case "schuman-declaration":
        return qsTr("Schuman Declaration")
    case "treaty-of-frankfurt":
        return qsTr("Treaty of Frankfurt")
    case "treaty-of-vereeniging":
        return qsTr("Treaty of Vereeniging ends the Second Boer War")
    case "treaty-of-trianon":
        return qsTr("Treaty of Trianon")
    case "congress-of-vienna":
        return qsTr("Final Act of the Congress of Vienna")
    case "un-charter":
        return qsTr("Charter of the United Nations signed")
    case "treaty-of-versailles":
        return qsTr("Treaty of Versailles")
    case "treaty-of-lausanne":
        return qsTr("Treaty of Lausanne")
    case "korean-armistice":
        return qsTr("Korean Armistice Agreement")
    case "peace-of-breda":
        return qsTr("Peace of Breda")
    case "partial-test-ban-treaty":
        return qsTr("Partial Nuclear Test Ban Treaty")
    case "peace-of-prague-1866":
        return qsTr("Peace of Prague")
    case "kellogg-briand-pact":
        return qsTr("Kellogg-Briand Pact outlaws war")
    case "treaty-of-nanking":
        return qsTr("Treaty of Nanking ends the First Opium War")
    case "japan-surrenders":
        return qsTr("Japan surrenders: the Second World War ends")
    case "treaty-of-paris-1783":
        return qsTr("Treaty of Paris ends the American Revolutionary War")
    case "treaty-of-portsmouth":
        return qsTr("Treaty of Portsmouth ends the Russo-Japanese War")
    case "two-plus-four-treaty":
        return qsTr("Two Plus Four Treaty")
    case "oslo-accords":
        return qsTr("Oslo Accords")
    case "camp-david-accords":
        return qsTr("Camp David Accords")
    case "peace-of-augsburg":
        return qsTr("Peace of Augsburg")
    case "treaty-of-aix-la-chapelle":
        return qsTr("Treaty of Aix-la-Chapelle")
    case "peace-of-westphalia":
        return qsTr("Peace of Westphalia ends the Thirty Years' War")
    case "armistice-1918":
        return qsTr("Armistice ends the First World War")
    case "inf-treaty":
        return qsTr("INF Treaty on intermediate-range missiles")
    case "universal-declaration-of-human-rights":
        return qsTr("Universal Declaration of Human Rights")
    case "dayton-agreement":
        return qsTr("Dayton Agreement ends the Bosnian War")
    case "treaty-of-ghent":
        return qsTr("Treaty of Ghent ends the War of 1812")

    // revolutionary
    case "cuban-revolution":
        return qsTr("The Cuban Revolution triumphs")
    case "prague-spring":
        return qsTr("The Prague Spring begins")
    case "bloody-sunday-1905":
        return qsTr("Bloody Sunday sets off the Russian Revolution of 1905")
    case "egyptian-revolution-2011":
        return qsTr("The Egyptian Revolution begins")
    case "iranian-revolution":
        return qsTr("The Iranian Revolution triumphs")
    case "french-revolution-1848":
        return qsTr("The February Revolution topples France's king")
    case "people-power-revolution":
        return qsTr("People Power Revolution in the Philippines")
    case "february-revolution-1917":
        return qsTr("The February Revolution begins in Russia")
    case "salt-march":
        return qsTr("Gandhi sets off on the Salt March")
    case "hungarian-revolution-1848":
        return qsTr("The Hungarian Revolution of 1848 begins")
    case "march-revolution-berlin":
        return qsTr("Barricades in Berlin: the March Revolution")
    case "easter-rising":
        return qsTr("The Easter Rising in Dublin")
    case "carnation-revolution":
        return qsTr("The Carnation Revolution in Portugal")
    case "indian-rebellion":
        return qsTr("The Indian Rebellion begins")
    case "magna-carta":
        return qsTr("The barons force King John to seal Magna Carta")
    case "uprising-17-june":
        return qsTr("The uprising in East Germany")
    case "stonewall":
        return qsTr("The Stonewall uprising")
    case "bastille":
        return qsTr("The storming of the Bastille")
    case "july-revolution":
        return qsTr("The July Revolution in Paris")
    case "defenestration-1419":
        return qsTr("The First Defenestration of Prague")
    case "warsaw-uprising":
        return qsTr("The Warsaw Uprising")
    case "haitian-revolution":
        return qsTr("The Haitian Revolution begins")
    case "baltic-way":
        return qsTr("The Baltic Way")
    case "belgian-revolution":
        return qsTr("The Belgian Revolution begins")
    case "march-on-washington":
        return qsTr("The March on Washington")
    case "gdansk-agreement":
        return qsTr("The Gdańsk Agreement makes way for Solidarity")
    case "leipzig-monday-demonstration":
        return qsTr("The great Monday Demonstration in Leipzig")
    case "xinhai-revolution":
        return qsTr("The Wuchang Uprising begins the Xinhai Revolution")
    case "hungarian-revolution-1956":
        return qsTr("The Hungarian Revolution of 1956 begins")
    case "ninety-five-theses":
        return qsTr("Martin Luther publishes his 95 Theses")
    case "kiel-mutiny":
        return qsTr("The Kiel mutiny begins the German Revolution")
    case "glorious-revolution":
        return qsTr("William of Orange lands: the Glorious Revolution")
    case "october-revolution":
        return qsTr("The October Revolution in Russia")
    case "fall-of-the-berlin-wall":
        return qsTr("The fall of the Berlin Wall")
    case "velvet-revolution":
        return qsTr("The Velvet Revolution begins")
    case "mexican-revolution":
        return qsTr("The Mexican Revolution begins")
    case "orange-revolution":
        return qsTr("The Orange Revolution begins")
    case "november-uprising":
        return qsTr("The November Uprising in Poland")
    case "rosa-parks":
        return qsTr("Rosa Parks refuses to give up her seat")
    case "boston-tea-party":
        return qsTr("The Boston Tea Party")
    case "arab-spring":
        return qsTr("The protests in Tunisia begin the Arab Spring")
    case "romanian-revolution":
        return qsTr("The Romanian Revolution topples Ceaușescu")

    // conqueror
    case "fall-of-granada":
        return qsTr("Granada falls to Castile and Aragon")
    case "crossing-the-rubicon":
        return qsTr("Caesar crosses the Rubicon")
    case "overthrow-of-hawaii":
        return qsTr("The Kingdom of Hawaii is overthrown")
    case "fall-of-paris-1871":
        return qsTr("Paris surrenders to the German armies")
    case "fall-of-baghdad":
        return qsTr("Baghdad falls to the Mongols")
    case "fall-of-singapore":
        return qsTr("Singapore falls to Japan")
    case "fall-of-the-alamo":
        return qsTr("The Alamo falls")
    case "anschluss":
        return qsTr("Germany annexes Austria")
    case "occupation-of-prague":
        return qsTr("Germany occupies Prague")
    case "battle-of-yamen":
        return qsTr("The Mongols complete their conquest of China at Yamen")
    case "invasion-of-denmark-and-norway":
        return qsTr("Germany invades Denmark and Norway")
    case "sack-of-constantinople":
        return qsTr("Crusaders take Constantinople")
    case "battle-of-panipat":
        return qsTr("Babur wins at Panipat and founds the Mughal Empire")
    case "fall-of-saigon":
        return qsTr("Saigon falls")
    case "fall-of-berlin":
        return qsTr("Berlin falls to the Red Army")
    case "fall-of-acre":
        return qsTr("Acre, the last crusader city, falls")
    case "conquest-of-toledo":
        return qsTr("Castile takes Toledo")
    case "fall-of-constantinople":
        return qsTr("Constantinople falls to the Ottomans")
    case "fall-of-paris-1940":
        return qsTr("German troops enter Paris")
    case "battle-of-plassey":
        return qsTr("The British win Bengal at Plassey")
    case "conquest-of-cordoba":
        return qsTr("Castile takes Córdoba")
    case "conquest-of-algiers":
        return qsTr("France takes Algiers")
    case "siege-of-jerusalem-1099":
        return qsTr("Crusaders take Jerusalem")
    case "fall-of-tenochtitlan":
        return qsTr("Tenochtitlan falls to Cortés")
    case "sack-of-rome-410":
        return qsTr("The Visigoths sack Rome")
    case "battle-of-mohacs":
        return qsTr("The Ottomans win Hungary at Mohács")
    case "fall-of-rome-476":
        return qsTr("The last Western Roman emperor is deposed")
    case "battle-of-the-plains-of-abraham":
        return qsTr("The British take Quebec")
    case "napoleon-in-moscow":
        return qsTr("Napoleon enters Moscow")
    case "capture-of-rome-1870":
        return qsTr("Italy takes Rome")
    case "fall-of-warsaw":
        return qsTr("Warsaw surrenders to Germany")
    case "battle-of-gaugamela":
        return qsTr("Alexander the Great defeats Persia at Gaugamela")
    case "saladin-takes-jerusalem":
        return qsTr("Saladin takes Jerusalem")
    case "fall-of-babylon":
        return qsTr("Cyrus the Great takes Babylon")
    case "battle-of-hastings":
        return qsTr("William the Conqueror wins at Hastings")
    case "conquest-of-lisbon":
        return qsTr("Portugal takes Lisbon")
    case "napoleon-in-vienna":
        return qsTr("Napoleon enters Vienna")
    case "battle-of-cajamarca":
        return qsTr("Pizarro captures the Inca emperor at Cajamarca")
    case "conquest-of-seville":
        return qsTr("Castile takes Seville")
    case "conquest-of-goa":
        return qsTr("Portugal takes Goa")
    case "fall-of-kyiv":
        return qsTr("Kyiv falls to the Mongols")
    case "sack-of-delhi":
        return qsTr("Timur takes Delhi")

    // explorer
    case "discovery-of-ceres":
        return qsTr("Piazzi discovers Ceres")
    case "antarctic-circle":
        return qsTr("Cook is the first to cross the Antarctic Circle")
    case "cook-reaches-hawaii":
        return qsTr("Cook reaches Hawaii")
    case "landing-on-antarctica":
        return qsTr("The first confirmed landing on Antarctica")
    case "cape-of-good-hope":
        return qsTr("Dias rounds the Cape of Good Hope")
    case "discovery-of-pluto":
        return qsTr("Tombaugh discovers Pluto")
    case "discovery-of-uranus":
        return qsTr("Herschel discovers Uranus")
    case "terracotta-army":
        return qsTr("Farmers find the Terracotta Army")
    case "ponce-de-leon-florida":
        return qsTr("Ponce de León reaches Florida")
    case "easter-island":
        return qsTr("Roggeveen reaches Easter Island")
    case "farthest-north":
        return qsTr("Nansen comes closer to the North Pole than anyone before")
    case "cabral-brazil":
        return qsTr("Cabral reaches Brazil")
    case "botany-bay":
        return qsTr("Cook lands at Botany Bay")
    case "de-soto-mississippi":
        return qsTr("De Soto reaches the Mississippi")
    case "north-pole-flight":
        return qsTr("The airship Norge flies over the North Pole")
    case "vasco-da-gama-india":
        return qsTr("Vasco da Gama reaches India by sea")
    case "chimborazo":
        return qsTr("Humboldt climbs higher on Chimborazo than anyone before")
    case "founding-of-quebec":
        return qsTr("Champlain founds Quebec")
    case "rosetta-stone":
        return qsTr("The Rosetta Stone is found")
    case "mackenzie-pacific":
        return qsTr("Mackenzie crosses North America to the Pacific")
    case "machu-picchu":
        return qsTr("Bingham reaches Machu Picchu")
    case "source-of-the-nile":
        return qsTr("Speke finds the source of the Nile")
    case "petra":
        return qsTr("Burckhardt rediscovers Petra")
    case "orellana-amazon":
        return qsTr("Orellana sails the Amazon to the Atlantic")
    case "northwest-passage":
        return qsTr("Amundsen completes the Northwest Passage")
    case "titanic-wreck":
        return qsTr("The wreck of the Titanic is found")
    case "first-circumnavigation":
        return qsTr("Elcano completes the first voyage around the world")
    case "lascaux":
        return qsTr("Teenagers find the cave paintings of Lascaux")
    case "darwin-galapagos":
        return qsTr("Darwin reaches the Galápagos Islands")
    case "otzi":
        return qsTr("Hikers find Ötzi in the Alps")
    case "discovery-of-neptune":
        return qsTr("Galle discovers Neptune")
    case "balboa-pacific":
        return qsTr("Balboa sights the Pacific")
    case "first-exoplanet":
        return qsTr("The first planet of a Sun-like star is found")
    case "columbus-americas":
        return qsTr("Columbus reaches the Americas")
    case "tutankhamun":
        return qsTr("Carter finds Tutankhamun's tomb")
    case "lewis-and-clark-pacific":
        return qsTr("Lewis and Clark see the Pacific")
    case "stanley-livingstone":
        return qsTr("Stanley finds Livingstone")
    case "victoria-falls":
        return qsTr("Livingstone reaches the Victoria Falls")
    case "tasmania":
        return qsTr("Tasman reaches Tasmania")
    case "magellan-pacific":
        return qsTr("Magellan sails into the Pacific")
    case "new-zealand":
        return qsTr("Tasman sights New Zealand")
    case "south-pole":
        return qsTr("Amundsen reaches the South Pole")

    // first-one
    case "jupiter-moons":
        return qsTr("Galileo sees the moons of Jupiter")
    case "deepest-dive":
        return qsTr("The first dive to the deepest point of the ocean")
    case "dolly-the-sheep":
        return qsTr("Dolly, the first cloned mammal, is announced")
    case "first-telephone-call":
        return qsTr("The first telephone call")
    case "first-spacewalk":
        return qsTr("The first spacewalk")
    case "black-hole-image":
        return qsTr("The first image of a black hole")
    case "first-human-in-space":
        return qsTr("Gagarin is the first human in space")
    case "splitting-the-atom":
        return qsTr("The first atom split by accelerated particles")
    case "first-space-station":
        return qsTr("The first space station is launched")
    case "dna-double-helix":
        return qsTr("The double helix of DNA is published")
    case "four-minute-mile":
        return qsTr("The first mile run in under four minutes")
    case "diamond-sutra":
        return qsTr("The Diamond Sutra, the oldest dated printed book")
    case "first-vaccination":
        return qsTr("Jenner gives the first vaccination")
    case "first-solo-transatlantic-flight":
        return qsTr("Lindbergh flies the Atlantic alone")
    case "everest":
        return qsTr("The first ascent of Mount Everest")
    case "first-nonstop-transatlantic-flight":
        return qsTr("The first nonstop flight across the Atlantic")
    case "first-woman-in-space":
        return qsTr("Tereshkova is the first woman in space")
    case "first-rocket-in-space":
        return qsTr("The first rocket reaches space")
    case "human-genome":
        return qsTr("The first draft of the human genome")
    case "special-relativity":
        return qsTr("Einstein submits his special theory of relativity")
    case "principia":
        return qsTr("Newton publishes his laws of motion and gravitation")
    case "moon-landing":
        return qsTr("The first humans land on the Moon")
    case "first-ivf-baby":
        return qsTr("The first child conceived in a test tube is born")
    case "mont-blanc":
        return qsTr("The first ascent of Mont Blanc")
    case "first-jet-flight":
        return qsTr("The first flight of a jet aircraft")
    case "luna-2":
        return qsTr("The first spacecraft reaches the Moon")
    case "gravitational-waves":
        return qsTr("Gravitational waves are detected for the first time")
    case "penicillin":
        return qsTr("Fleming discovers penicillin")
    case "sputnik":
        return qsTr("Sputnik, the first artificial satellite")
    case "sound-barrier":
        return qsTr("The first flight faster than sound")
    case "first-anaesthesia":
        return qsTr("The first public operation under anaesthesia")
    case "x-rays":
        return qsTr("Röntgen discovers X-rays")
    case "first-crewed-balloon-flight":
        return qsTr("The first crewed balloon flight")
    case "origin-of-species":
        return qsTr("Darwin publishes On the Origin of Species")
    case "first-nuclear-reactor":
        return qsTr("The first nuclear chain reaction")
    case "first-heart-transplant":
        return qsTr("The first heart transplant")
    case "first-nobel-prizes":
        return qsTr("The first Nobel Prizes are awarded")
    case "first-transatlantic-radio":
        return qsTr("The first radio signal across the Atlantic")
    case "first-powered-flight":
        return qsTr("The Wright brothers make the first powered flight")
    case "first-rocket-landing":
        return qsTr("An orbital rocket lands upright for the first time")
    case "first-kidney-transplant":
        return qsTr("The first successful kidney transplant")
    case "apollo-8":
        return qsTr("The first humans orbit the Moon")

    // innovator
    case "steam-engine":
        return qsTr("Watt patents his steam engine")
    case "daguerreotype":
        return qsTr("Photography by daguerreotype is announced")
    case "jet-engine":
        return qsTr("Whittle files his patent for the jet engine")
    case "television":
        return qsTr("Baird demonstrates television")
    case "automobile":
        return qsTr("Benz patents his motor car")
    case "eniac":
        return qsTr("ENIAC, an electronic computer, is unveiled")
    case "phonograph":
        return qsTr("Edison patents the phonograph")
    case "steam-locomotive":
        return qsTr("The first steam locomotive runs on rails")
    case "gps":
        return qsTr("The first GPS satellite is launched")
    case "diesel-engine":
        return qsTr("Diesel is granted his patent for the engine")
    case "nylon":
        return qsTr("Nylon is first made")
    case "telephone":
        return qsTr("Bell is granted his patent for the telephone")
    case "world-wide-web":
        return qsTr("Berners-Lee proposes the World Wide Web")
    case "cotton-gin":
        return qsTr("Whitney patents the cotton gin")
    case "liquid-fuel-rocket":
        return qsTr("Goddard launches the first liquid-fuelled rocket")
    case "battery":
        return qsTr("Volta describes his battery")
    case "mobile-phone":
        return qsTr("The first call from a handheld mobile phone")
    case "safety-pin":
        return qsTr("Hunt patents the safety pin")
    case "dynamite":
        return qsTr("Nobel patents dynamite")
    case "z3-computer":
        return qsTr("Zuse presents the Z3, the first programmable computer")
    case "laser":
        return qsTr("Maiman builds the first laser")
    case "blue-jeans":
        return qsTr("Strauss and Davis patent riveted jeans")
    case "telegraph":
        return qsTr("Morse sends his first telegram over a long line")
    case "radio":
        return qsTr("Marconi applies for his patent for the radio")
    case "bicycle":
        return qsTr("Drais rides his running machine, the bicycle's ancestor")
    case "typewriter":
        return qsTr("Sholes patents the typewriter")
    case "zeppelin":
        return qsTr("The first Zeppelin takes off")
    case "telstar":
        return qsTr("Telstar, the first active communications satellite")
    case "aspirin":
        return qsTr("Hoffmann makes pure aspirin")
    case "sewing-machine":
        return qsTr("Howe patents the sewing machine")
    case "integrated-circuit":
        return qsTr("Kilby demonstrates the integrated circuit")
    case "barcode":
        return qsTr("The barcode is patented")
    case "microwave-oven":
        return qsTr("Spencer files his patent for the microwave oven")
    case "light-bulb":
        return qsTr("Edison's light bulb burns for hours")
    case "internet":
        return qsTr("The first message over the ARPANET, the internet's forerunner")
    case "microprocessor":
        return qsTr("The first microprocessor goes on sale")
    case "assembly-line":
        return qsTr("Ford starts the moving assembly line")
    case "text-message":
        return qsTr("The first text message is sent")
    case "pneumatic-tyre":
        return qsTr("Dunlop is granted his patent for the pneumatic tyre")
    case "computer-mouse":
        return qsTr("Engelbart demonstrates the computer mouse")
    case "transistor":
        return qsTr("The first transistor works")
    case "cinematograph":
        return qsTr("The Lumière brothers show the first films to the public")

    // virtuoso
    case "birth-elvis-presley":
        return qsTr("Elvis Presley is born")
    case "birth-mozart":
        return qsTr("Wolfgang Amadeus Mozart is born")
    case "birth-schubert":
        return qsTr("Franz Schubert is born")
    case "birth-mendelssohn":
        return qsTr("Felix Mendelssohn is born")
    case "birth-bob-marley":
        return qsTr("Bob Marley is born")
    case "birth-handel":
        return qsTr("George Frideric Handel is born")
    case "birth-chopin":
        return qsTr("Frédéric Chopin is born")
    case "birth-smetana":
        return qsTr("Bedřich Smetana is born")
    case "birth-vivaldi":
        return qsTr("Antonio Vivaldi is born")
    case "birth-ravel":
        return qsTr("Maurice Ravel is born")
    case "birth-bach":
        return qsTr("Johann Sebastian Bach is born")
    case "birth-bartok":
        return qsTr("Béla Bartók is born")
    case "birth-haydn":
        return qsTr("Joseph Haydn is born")
    case "birth-rachmaninoff":
        return qsTr("Sergei Rachmaninoff is born")
    case "birth-duke-ellington":
        return qsTr("Duke Ellington is born")
    case "birth-tchaikovsky":
        return qsTr("Pyotr Ilyich Tchaikovsky is born")
    case "birth-monteverdi":
        return qsTr("Claudio Monteverdi is baptised")
    case "birth-wagner":
        return qsTr("Richard Wagner is born")
    case "birth-elgar":
        return qsTr("Edward Elgar is born")
    case "birth-schumann":
        return qsTr("Robert Schumann is born")
    case "birth-richard-strauss":
        return qsTr("Richard Strauss is born")
    case "birth-grieg":
        return qsTr("Edvard Grieg is born")
    case "birth-stravinsky":
        return qsTr("Igor Stravinsky is born")
    case "birth-mahler":
        return qsTr("Gustav Mahler is born")
    case "birth-louis-armstrong":
        return qsTr("Louis Armstrong is born")
    case "birth-debussy":
        return qsTr("Claude Debussy is born")
    case "birth-bernstein":
        return qsTr("Leonard Bernstein is born")
    case "birth-bruckner":
        return qsTr("Anton Bruckner is born")
    case "birth-freddie-mercury":
        return qsTr("Freddie Mercury is born")
    case "birth-dvorak":
        return qsTr("Antonín Dvořák is born")
    case "birth-clara-schumann":
        return qsTr("Clara Schumann is born")
    case "birth-shostakovich":
        return qsTr("Dmitri Shostakovich is born")
    case "birth-gershwin":
        return qsTr("George Gershwin is born")
    case "birth-john-lennon":
        return qsTr("John Lennon is born")
    case "birth-verdi":
        return qsTr("Giuseppe Verdi is born")
    case "birth-liszt":
        return qsTr("Franz Liszt is born")
    case "birth-johann-strauss":
        return qsTr("Johann Strauss II is born")
    case "birth-paganini":
        return qsTr("Niccolò Paganini is born")
    case "birth-sibelius":
        return qsTr("Jean Sibelius is born")
    case "birth-berlioz":
        return qsTr("Hector Berlioz is born")
    case "birth-beethoven":
        return qsTr("Ludwig van Beethoven is born")
    case "birth-puccini":
        return qsTr("Giacomo Puccini is born")
    default:
        return key
    }
}
